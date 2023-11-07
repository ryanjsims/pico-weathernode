from weecfg.extension import ExtensionInstaller

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