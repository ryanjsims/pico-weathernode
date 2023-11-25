# Pico Weathernode

This is a Raspberry Pi Pico W project meant to act as the controller for a basic wifi attached weather station.

To initialize submodules, run this command in the repo's root folder
```bash
git submodule update --init && cd lib/pico-sdk && git submodule update --init && cd ../pico-web-client && git submodule update --init lib/json && cd ../..
```