# Battery-powered Feather M0 LoRaWAN particular matter sensor 

## Motivation

Battery powered replacement for the famous [airrohr](https://sensor.community/en/sensors/airrohr/)
by the sensor.community (formerly known as luftdaten.info). Please note that this sensor cannot
transmit the PM readings to a public sensor network like the [sensor.community](https://sensor.community/)
or [openSenseMap](https://sensebox.de/en/opensensemap) on its own, since it doesn't have a WiFi uplink
link. You need to deploy you own software to retrieve the senosr data from your LoRaWAN provider
(e.g. [TTN](https://www.thethingsnetwork.org/)) and forward it to public database.

## Features

- measures particulate matter (PM 2.5 and PM 10), humidity and temperature
- sensor readings are transmitted at preset intervals (e.g. ever 10 minutes) using LoRaWAN
- the SDS011 sensor is powered up for 20 seconds before reading the measured values
- supports BME280, Si7032 and SHT31 as temperature/humidity sensor
- Feather M0 sleeps between sensor readings to save power (~5 mA)
- battery-powered to be operated away from a power socket (airrohr needs 5V power supply)

## Hardware components

- [Feather M0 with RFM95 LoRa Radio](https://learn.adafruit.com/adafruit-feather-m0-radio-with-lora-radio-module/overview)
- Nova Fitness SDS011 Fine Dust Sensor
- temperature/humidityp/pressure I2C sensor [BME280](https://www.bosch-sensortec.com/products/environmental-sensors/humidity-sensors-bme280/)
- 3.7V Lithium Polymer or Lithium Ion battery (>2000mAh) with JST jack

## Upload firmware

To compile the firmware for the Feather M0 LoRa module just download [Visual
Studio Code](https://code.visualstudio.com/) and install the [PlatformIO
add-on](https://platformio.org/install/ide?install=vscode). Open the project
directory and adjust the settings in `include/config.h` to your needs (set
LoRaWAN DevEUI, JoinEUI and AppKey).

You need to remove or comment out the `os_getBattLevel()` function at line 122
in the file `src/lmic/lmic.c` of the MCCI LoRaWAN LMIC Library to avoid a linker
error about `multiple definitions` of this function call.

## Contributing

Pull requests are welcome! For major changes, please open an issue first
to discuss what you would like to change.

## License

Copyright (c) 2024 Lars Wessels  
This software was published under the Apache License 2.0.  
Please check the [license file](LICENSE).
