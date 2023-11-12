import weewx.drivers
from weewx.manager import Manager
import weecfg
from configobj import ConfigObj, Section
from multiprocessing import Process, Queue
import queue as pyQueue
import socketio
import eventlet
import eventlet.wsgi
import os, signal
import time
import logging

from flask import Flask, Blueprint
from werkzeug.middleware.proxy_fix import ProxyFix

DRIVER_NAME = "Weathernode"
DRIVER_VERSION  = "0.1"

log = logging.getLogger(DRIVER_NAME + " Extension v" + DRIVER_VERSION)
sio_log = logging.getLogger(f"{DRIVER_NAME} v{DRIVER_VERSION} SocketIO")
http_log = logging.getLogger(f"{DRIVER_NAME} v{DRIVER_VERSION} HTTP")

queue = Queue()
sio = socketio.Server()
http = Flask(__name__)
bp = Blueprint('API', __name__, url_prefix='/api')

manager: Manager = None

class LoopPacket:
    def __init__(self, **kwargs):
        self.__packet = {
            "UV": None,
            "barometer": None,
            "consBatteryVoltage": None,
            "heatingVoltage": None,
            "inHumidity": None,
            "inTemp": None,
            "inTempBatteryStatus": None,
            "outHumidity": None,
            "outTemp": None,
            "outTempBatteryStatus": None,
            "pressure": None,
            "radiation": None,
            "rain": None,
            "rainBatteryStatus": None,
            "referenceVoltage": None,
            "rxCheckPercent": None,
            "supplyVoltage": None,
            "txBatteryStatus": None,
            "windBatteryStatus": None,
            "windDir": None,
            "windGust": None,
            "windGustDir": None,
            "windSpeed": None
        }
        for arg in kwargs:
            if arg in self.__packet:
                self.__packet[arg] = kwargs[arg]
    
    def serialize(self):
        self.__packet["dateTime"] = int(time.time())
        self.__packet["usUnits"] = weewx.METRIC
        return self.__packet

@sio.event
def connect(sid, environ, auth):
    sio_log.info(f"Connection from {sid}")

@sio.event
def disconnect(sid):
    sio_log.info(f"Disconnecting {sid}")

@sio.event
def weather_event(sid, *data):
    queue.put(data[0])

def to_celsius(fahrenheit: float|None) -> float|None:
    if not fahrenheit:
        return None
    return (fahrenheit - 32) * 5.0/9.0

@bp.get("/temperature")
def temperature():
    global manager
    if not manager:
        return {
            "fahrenheit": None,
            "celsius": None
        }
    latest = manager.getRecord(manager.lastGoodStamp())
    http_log.info(f"Returning latest temperature of {latest.get('outTemp'):.2f}°F")
    return {
        "fahrenheit": latest.get("outTemp"),
        "celsius": to_celsius(latest.get("outTemp"))
    }


http.register_blueprint(bp)

def run_socketio(port: int):
    log = logging.getLogger(__name__)
    app = socketio.WSGIApp(sio)
    socket = eventlet.listen(('', port))
    socket.settimeout(None)
    eventlet.wsgi.server(socket, app, log)

def run_http(port: int):
    global manager
    db_dict = {}
    _, config = weecfg.read_config(None)
    binding: Section = config.get("DataBindings")["wx_binding"]
    db: Section = config.get("Databases")[binding.get("database")]
    db_type: Section = config.get("DatabaseTypes")[db.get("database_type")]
    db_dict.update(db_type.dict())
    db_dict["database_name"] = db.get("database_name")
    manager = Manager.open(db_dict, binding.get("table_name"))
    http.wsgi_app = ProxyFix(http.wsgi_app, x_host=1, x_prefix=1)
    socket = eventlet.listen(('', port))
    socket.settimeout(None)
    eventlet.wsgi.server(socket, http, logging.getLogger(__name__))

def loader(config_dict, engine):
    return PicoWeathernode(**config_dict[DRIVER_NAME])

class PicoWeathernode(weewx.drivers.AbstractDevice):
    def __init__(self, **config):
        self.__sio_port = int(config["sio_port"])
        self.__http_port = int(config["http_port"])
        log.info("Starting socketio server...")
        self.__sio_process = Process(target=run_socketio, args=[self.__sio_port], daemon=True)
        self.__sio_process.start()
        log.info("Starting http server...")
        self.__http_process = Process(target=run_http, args=[self.__http_port], daemon=True)
        self.__http_process.start()

    def genLoopPackets(self):
        while True:
            if self.__sio_process.exitcode:
                self.__sio_process = Process(target=run_socketio, args=[self.__sio_port], daemon=True)
                self.__sio_process.start()
            if self.__http_process.exitcode:
                self.__http_process = Process(target=run_http, args=[self.__http_port], daemon=True)
                self.__http_process.start()
            try:
                data = queue.get(timeout=1)
                packet = LoopPacket(**data)
                yield packet.serialize()
            except pyQueue.Empty:
                pass

    def closePort(self):
        log.info("Closing")
        os.kill(self.__sio_process.pid, signal.SIGINT)

    @property
    def hardware_name(self):
        return "Weathernode"


def confeditor_loader():
    return PicoWeathernodeConfEditor()


class PicoWeathernodeConfEditor(weewx.drivers.AbstractConfEditor):
    @property
    def default_stanza(self):
        return """
[Weathernode]
    # This section is for https://github.com/ryanjsims/pico-weathernode sensors
    # The weathernode driver hosts a socketio server which the sensor will connect with
    # to send data to weewx. Any time a packet is received, a loop packet will be generated

    # The driver to use:
    driver = user.weathernode_driver

    # The port to listen on for socketio connections
    sio_port = 9834
    # The port to listen on for http connections
    http_port = 9835
"""

if __name__ == "__main__":
    station = PicoWeathernode(sio_port=9834, http_port=9835)
    for packet in station.genLoopPackets():
        print(packet)