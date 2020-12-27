# Zero ODB-II 
ESP8266 ESP-01 OBD-II Microcontroller for Zero Motorcycles
Allows you to monitor your Zero Motorcycle's specs (battery voltage, pack voltage, temperatures, etc) through an HTTP interface.

### Features
 * Provides a JSON blob of data to HTTP clients at `/json`
 * Allow HTTP clients to run some commands against the OBD interface
 * Garage door control through [garage_door_closer](https://github.com/patfreeman/garage_door_closer)

### Hardware Setup
1. ESP-01
2. Male OBD connector
3. Some wiring-fu from ESP-01 to the [OBD port](https://zeromanual.com/wiki/How_to_build_a_cable_to_access_the_MBB
