from weecfg.extension import ExtensionInstaller
import setuptools

def loader():
    return WeathernodeInstaller()

class WeathernodeInstaller(ExtensionInstaller):
    def __init__(self):
        super(WeathernodeInstaller, self).__init__(
            version="0.1",
            name="weathernode_driver",
            description="Pico Weathernode Socketio server for weewx.",
            author="Ryan Sims",
            author_email="",
            config={
                'Station': {
                    'station_type': 'Weathernode'
                },
                'Weathernode': {
                    'driver': 'user.weathernode_driver',
                    'port': '9834'
                }
            },
            files=[('bin/user', ['bin/user/weathernode_driver.py'])]
        )
        setuptools.setup(install_requires=[
            "bidict==0.22.1",
            "dnspython==2.4.2",
            "eventlet==0.33.3",
            "greenlet==3.0.1",
            "h11==0.14.0",
            "python-engineio==4.8.0",
            "python-socketio==5.10.0",
            "simple-websocket==1.0.0",
            "six==1.16.0",
            "wsproto==1.2.0"
        ])