/***************************************************************************
  Copyright (c) 2024 Lars Wessels

  This file is part of the "Feather-M0-LoRaWAN-PM-Sensor" source code.
  https://github.com/lrswss/feather-m0-lorawan-pm-sensor

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
   
  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

***************************************************************************/

#include "sensors.h"
#include "utils.h"
#include "config.h"

#define I2C Wire

sensorReadings_t sensorReadings = { 
        -99.0, // temp
        -1,    // pressure
        -1,    // humidity
        -1,    // pm2.5
        -1.0,  // pm10
        0.0,   // vbat
        SENSORS_OFFLINE
    };


Adafruit_BME280 bme = Adafruit_BME280();
Adafruit_SHT31 sht31 = Adafruit_SHT31(&I2C);
Adafruit_Si7021 si7021 = Adafruit_Si7021(&I2C);
SDS011 sds = SDS011(WARMUP_SECS);


// read battery voltage using voltage divider
bool vbat_read(bool verbose) {
#ifdef VBAT_PIN
    static char buf[8];

    sensorReadings.vbat  = (analogRead(VBAT_PIN) * VBAT_MULTIPLIER * 3.3) / 1024;
    dtostrf(sensorReadings.vbat, 4, 2, buf);
    if (sensorReadings.vbat > 2.55 && sensorReadings.vbat <= VBAT_MAX_LEVEL) {
        if (verbose) {
            if (sensorReadings.vbat <= VBAT_MIN_LEVEL)
                log_msg("[WARNING] low battery voltage: %s V", buf);
            else
                log_msg("Battery voltage: %s V", buf);
        }
        return true;
    } else {
        sensorReadings.vbat = 0.0;
        if (verbose) {
            log_msg("No battery connected or currently charging");
        }
        return false;
    }
#endif
    return false;
}


// start and scan I2C for devices
// return number of devices found
static uint8_t i2c_init() {
  uint8_t addr, error, devices = 0;

    I2C.begin();
    log_msg("Scanning I2C bus...");
    for (addr = 1; addr < 127; addr++) {
        I2C.beginTransmission(addr);
        error = I2C.endTransmission();
        if (error == 0) {
            log_msg("Found I2C device at address 0x%02X", addr);
            devices++;
            if (devices > 5) {
                log_msg("[WARNING] Too many I2C devices detected (I2C bus error)");
                sensorReadings.status |= SENSORS_I2C_ERROR;
                return 0;
            }
        } else if (error == 4) {
            log_msg("[WARNING] Unknown I2C error for device address 0x%02X", addr);
            sensorReadings.status |= SENSORS_I2C_ERROR;
            return 0;
        }
    }
    if (devices == 0)
        log_msg("[WARNING] No I2C devices found!");
    else
        log_msg("Found %d devices", devices);
    return devices;
}


// initialize BME-Sensor
static bool bme280_init() {
    if (!bme.begin(BMP_BME280_ADDRESS, &I2C)) {
        log_msg("Sensor BMP280 or BME280 not found!");
        return false;
    }

    switch (bme.sensorID()) {
        case 0x60:
            log_msg("Sensor BME280 (Temp/Hum/Pres) ready");
            sensorReadings.status |= SENSORS_HAS_BME280;
            return true;
        default:
            log_msg("Found UNKNOWN sensor!");
            return false;
    }

    // setting for weather station
    // suggested rate is 1/60Hz (1m)
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1, // temperature
                    Adafruit_BME280::SAMPLING_X1, // pressure
                    Adafruit_BME280::SAMPLING_X1, // humidity
                    Adafruit_BME280::FILTER_OFF);

    return true;
}


// put BME280 to sleep
// https://github.com/G6EJD/BME280-Sleep-and-Address-change
static void bme280_sleep() {
    I2C.beginTransmission(BME280_ADDRESS);
    I2C.write((uint8_t)0xF4);
    I2C.write((uint8_t)0b00000000);
    I2C.endTransmission();
}


// initialize SHT31 temperature/humidity sensor
static bool sht31_init() {
    if (!sht31.begin(SHT31_ADDRESS)) {
        log_msg("Sensor SHT31 not found!");
        return false;
    }
    log_msg("Sensor SHT31 (Temp/Hum) ready");
    sensorReadings.status |= SENSORS_HAS_SHT31;
    return true;
}


static bool sds011_init() {
    char version[8];
    uint16_t sensorid;

    sds.begin();
    if (sds.info(version, sensorid)) {
        log_msg("Sensor SDS011 %d v%s (PM2.5/PM10) ready", sensorid, version);
        return true;
    }
    log_msg("Sensor SDS011 not found!") ;
    return false;
}


// initialize SI7021 temperature/humidity sensor
static bool si7021_init() {
    if (!si7021.begin()) {
        log_msg("Sensor SI7021 not found!");
        return false;
    }
    log_msg("Sensor SI7021 v%d (Temp/Hum) ready", si7021.getRevision());
    sensorReadings.status |= SENSORS_HAS_SI7021;
    return true;
}


// get readings for BME280 (temperature, humidity, pressure)
static void bme280_readings(bool verbose) {
    bme.takeForcedMeasurement();
    sensorReadings.pressure = bme.readPressure() / 100.0F;  // hPa
    sensorReadings.temperature = bme.readTemperature();  // °C
    sensorReadings.humidity = int(bme.readHumidity()); // %

    if (isnan(sensorReadings.temperature)) {
        serial.println("BME280: failed to read temperature!");
        sensorReadings.temperature = -99.0;
    }
    if (isnan(sensorReadings.humidity)) {
        serial.println("BME280: failed to read humidity!");
        sensorReadings.humidity = -1.0;
    }
    if (isnan(sensorReadings.pressure)) {
        serial.println("BME280: failed to read pressure!");
        sensorReadings.humidity = -1.0;
    }

    if (!verbose)
        return;
    serial.print("- Temperature: ");
    serial.print(sensorReadings.temperature, 2);
    serial.println(" C");
    serial.print("- Humidity: ");
    serial.print(sensorReadings.humidity);
    serial.println(" %");
    serial.print("- Pressure: ");
    serial.print(sensorReadings.pressure, 1);
    serial.println(" hPa");
}


// get readings for (temperature, humidity) from SHT31
static void sht31_readings(bool verbose) {
    sensorReadings.temperature = sht31.readTemperature();
    sensorReadings.humidity = sht31.readHumidity();
    if (isnan(sensorReadings.temperature)) {
        serial.println("SHT31: failed to read temperature!");
        sensorReadings.temperature = -99.0;
    }
    if (isnan(sensorReadings.humidity)) {
        serial.println("SHT31: failed to read humidity!");
        sensorReadings.humidity = -1.0;
    }

    if (!verbose)
        return;
    serial.print(F("- Temperature: "));
    serial.print(sensorReadings.temperature, 2);
    serial.println(" C");
    serial.print(F("- Humidity: "));
    serial.print(sensorReadings.humidity);
    serial.println(F(" %"));
}


// get readings for (temperature, humidity) from SI7021
static void si7021_readings(bool verbose) {
    sensorReadings.temperature = si7021.readTemperature();
    sensorReadings.humidity = si7021.readHumidity();
    if (isnan(sensorReadings.temperature)) {
        serial.println("SI7021: failed to read temperature!");
        sensorReadings.temperature = -99.0;
    }
    if (isnan(sensorReadings.humidity)) {
        serial.println("SI7021: failed to read humidity!");
        sensorReadings.humidity = -1.0;
    }

    if (!verbose)
        return;
    serial.print(F("- Temperature: "));
    serial.print(sensorReadings.temperature, 2);
    serial.println(" C");
    serial.print(F("- Humidity: "));
    serial.print(sensorReadings.humidity);
    serial.println(F(" %"));
}


static void sds011_readings(bool verbose) {
    sds.poll(&sensorReadings.pm25, &sensorReadings.pm10, AVG_READINGS);

    if (!verbose)
        return;
    serial.print(F("- PM 2.5: "));
    serial.print(sensorReadings.pm25, 2);
    serial.println(" μg/m3");
    serial.print(F("- PM 10: "));
    serial.print(sensorReadings.pm10, 2);
    serial.println(F(" μg/m3"));

}


// start I2C bus and check for (right) number of devices
// and initialize all available sensors
void sensors_init() {
    uint8_t devices = i2c_init();
    if (devices == 0)
        return;
    if (devices > 1)
        log_msg("[WARNING] Found more than one I2C sensor, will only use one!");
    if (!bme280_init()) {
        if (!sht31_init()) {
            if (!si7021_init())
                sensorReadings.status |= SENSORS_I2C_FAILED;
        }
    }
    if (!sds011_init())
        sensorReadings.status |= SENSORS_SDS011_ERROR;
    sensorReadings.status |= SENSORS_INITED;
}


// fill global struct sensorReadings with current value
// returns true if current wind speed is available
void sensors_read(bool verbose) {
    log_msg("Reading sensors...");

    if ((sensorReadings.status & SENSORS_SDS011_ERROR) == 0)
        sds011_readings(verbose);
    if (sensorReadings.status & SENSORS_HAS_BME280) {
        bme280_readings(verbose);
        return;
    } else if (sensorReadings.status & SENSORS_HAS_SHT31) {
        sht31_readings(verbose);
        return;
    } else if (sensorReadings.status & SENSORS_HAS_SI7021) {
        si7021_readings(verbose);
        return;
    }
    log_msg("[WARNING] Skipping temperature/humidity readings, not ready!");
}


// check if sensors are reading for reading data
// SDS011 requires about 30 sec. warmup time after sleep mode
bool sensors_ready() {
    return (sensorReadings.status & SENSORS_SDS011_ERROR) == 0 && sds.ready();
}


// turn on or reset sensors
void sensors_warmup() {
    if ((sensorReadings.status & SENSORS_SDS011_ERROR) == 0)
        sds.wakeup();
    if (sensorReadings.status & SENSORS_HAS_SHT31)
        sht31.reset();
    if (sensorReadings.status & SENSORS_HAS_SI7021)
        si7021.reset();
    sensorReadings.status |= SENSORS_WARMUP;
}


// turn off sensors (if supported)
void sensors_off() {
    if ((sensorReadings.status & SENSORS_SDS011_ERROR) == 0)
        sds.sleep();
    if (sensorReadings.status & SENSORS_HAS_BME280)
        bme280_sleep();
    if ((sensorReadings.status & SENSORS_HAS_SHT31) && (sensorReadings.humidity > 90)) {
        sht31.heater(true);
        delay(1500);
        sht31.heater(false);
    }
    if ((sensorReadings.status & SENSORS_HAS_SI7021) && (sensorReadings.humidity > 90)) {
        si7021.heater(true);
        delay(1500);
        si7021.heater(false);
    }
    sensorReadings.status &= ~(SENSORS_WARMUP);
}


// returns true if sensors are not available
bool sensors_error() {
    return (sensorReadings.status & (SENSORS_I2C_ERROR|SENSORS_I2C_FAILED|SENSORS_SDS011_ERROR));
}