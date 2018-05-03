# pds_project
LED strip and motion sensor in HASS.IO via MQTT

### Wiring diagram
![wiring diagram](zapojenie.png)

### Description
Motion sensor - [MQTT binary sensor](https://www.home-assistant.io/components/binary_sensor.mqtt/)
LED strip - [MQTT JSON light](https://www.home-assistant.io/components/light.mqtt_json/) with following functions:

| Function | Support |
| ------ | ------ |
| Brightness | YES |
| Color temperature | - |
| Effects | YES |
| Flashing | YES |
| RGB Color | YES |
| Transitions | - |
| XY Color | - |
| White value | - |
