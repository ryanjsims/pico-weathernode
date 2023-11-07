import weewx.drivers
from multiprocessing import Process, Queue
import queue as pyQueue
import socketio
import eventlet
import eventlet.wsgi
import os, signal
import time
import logging

DRIVER_NAME = "Weathernode"
DRIVER_VERSION  = "0.1"

log = logging.getLogger(DRIVER_NAME + " Extension v" + DRIVER_VERSION)

queue = Queue()
sio = socketio.Server()

@sio.event
def weather_event(sid, *data):
    queue.put(data[0])

def run_server(port: int):
    log = logging.getLogger(__name__)
    app = socketio.WSGIApp(sio)
    eventlet.wsgi.server(eventlet.listen(('', port)), app, log)

def loader(config_dict, engine):
    return PicoWeathernode(**config_dict[DRIVER_NAME])

class PicoWeathernode(weewx.drivers.AbstractDevice):
    def __init__(self, **config):
        log.info("Starting socketio server...")
        self.__sio_process = Process(target=run_server, args=[int(config["port"])])
        self.__sio_process.start()

    def genLoopPackets(self):
        while True:
            try:
                packet = queue.get()
                packet["dateTime"] = int(time.time())
                packet["usUnits"] = weewx.METRIC
                yield packet
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

    # The port to listen on
    port = 9834
"""

if __name__ == "__main__":
    station = PicoWeathernode(port=9834)
    for packet in station.genLoopPackets():
        print(packet)