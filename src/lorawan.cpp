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

#include "config.h"
#include "lorawan.h"
#include "sensors.h"
#include "utils.h"
#include "rtc.h"

osjob_t observMsg;
lmic_states lmic_status = NONE;

const lmic_pinmap lmic_pins = {
    .nss = LORA_PIN_NSS,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = LORA_PIN_RST,
    .dio = { LORA_PIN_DIO0, LORA_PIN_DIO1, LMIC_UNUSED_PIN },
    .rxtx_rx_active = 0,
    .rssi_cal = 8,  // LBT cal for the Adafruit Feather M0 LoRa, in dB
    .spi_freq = 8000000,
};


// LoRaWAN OTAA keys are (pre)set in lorawan.h
static const uint8_t DEVEUI[8] = LORAWAN_DEV_EUI;
static const uint8_t APPEUI[8] = LORAWAN_APP_EUI;
static const uint8_t APPKEY[16] = LORAWAN_APP_KEY;


void os_getDevEui (u1_t* buf) {
    memcpy(buf, DEVEUI, 8);
}


void os_getArtEui (u1_t* buf) {
    memcpy(buf, APPEUI, 8);
}


void os_getDevKey (u1_t* buf) {
    memcpy(buf, APPKEY, 16);
}


// returns LMIC version number string
static char* lmic_version() {
    static char version[16];
    sprintf(version, "%lu.%lu.%lu.%lu",
        ARDUINO_LMIC_VERSION_GET_MAJOR(ARDUINO_LMIC_VERSION),
        ARDUINO_LMIC_VERSION_GET_MINOR(ARDUINO_LMIC_VERSION),
        ARDUINO_LMIC_VERSION_GET_PATCH(ARDUINO_LMIC_VERSION),
        ARDUINO_LMIC_VERSION_GET_LOCAL(ARDUINO_LMIC_VERSION));
    return version;
}


// print (preset) OTAA authentication keys
// stored in lorawanPrefs struct
static void print_otaa_data() {
    uint8_t buf[16];

    os_getDevEui(buf);
    serial.print("Device EUI: "); // stored in flash, cannot be changed
    print_hex(buf, sizeof(DEVEUI), true, true);
    os_getArtEui(buf);
    serial.print("Application EUI: ");
    print_hex(buf, sizeof(APPEUI), true, true);
    os_getDevKey(buf);
    serial.print("Application Key: ");
    print_hex(buf, sizeof(APPKEY), true, false);
    serial.flush();
}


// print temporary session keys
static void print_session_keys() {
    serial.printf("Netid: 0x%0X%s\n",
        (LMIC.netid & 0x001FFFFF), LMIC.netid == 0x13 ? " (TTN)" : "");
    serial.printf("Device Address: %06X\n", LMIC.devaddr);
    serial.print("App Session Key: ");
    print_hex(LMIC.artKey, sizeof(LMIC.artKey), true, false);
    serial.print("Network Session Key: ");
    print_hex(LMIC.nwkKey, sizeof(LMIC.nwkKey), true, false);
    serial.flush();
}


// request network time using DeviceTimeReq MAC command (LoRaWAN Spec v1.0.3)
// https://github.com/mcci-catena/arduino-lmic/pull/147
// requires #define LMIC_ENABLE_DeviceTimeReq 1 in lmic_project_config.h
#ifdef LORAWAN_NETWORKTIME
static void networkTimeCallback(void *pUserData, int success) {
    uint32_t *ts_sec = (uint32_t *)pUserData;
    lmic_time_reference_t lmicTimeRef;

    if (success != 1) {
        log_msg("networkTimeCallback() failed!");
        return;
    } else if (LMIC_getNetworkTimeReference(&lmicTimeRef) != 1) {
        log_msg("LMIC_getNetworkTimeReference failed!");
        return;
    }

    // adjust network time (based on GPS epoch) to UTC
    *ts_sec = lmicTimeRef.tNetwork + 315964800 - 18;

    // add delay since time request was sent
    *ts_sec += osticks2ms(os_getTime() - lmicTimeRef.tLocal) / 1000;

    // set RTC
    rtc.setEpoch(*ts_sec);
    log_msg("Set RTC to LoRaWAN network time");
}
#endif


// remove a scheduled LMIC tx job
static void lmic_remove(osjob_t* j) {
    os_clearCallback(j);
    j->deadline = 0; // to make os_jobIsTimed() work as expected
    j->func = [](osjob_t* j){ }; // os_clearCallback() does not remove jobs from OS.scheduled
    os_runloop_once(); // execute empty callback to remove job from OS.scheduled
}


// wait for pending LoRaWAN jobs (TXRX/JOIN)
static void lmic_pending_jobs(uint16_t waitSecs) {
    uint32_t wait = 0;
    bool once = false;

    while ((LMIC.opmode & (OP_TXRXPEND|OP_JOINING)) || os_queryTimeCriticalJobs(ms2osticks(waitSecs*1000))) {
        if (!once) {
            log_msg("LoRaWAN jobs pending, waiting for completion...");
            once = true;
        }
        if (wait > (waitSecs * 1000))
            break;
        os_runloop_once();
        delay(1);
    }
}


// start OTAA join
bool lmic_join(uint8_t repeat) {
    static uint8_t joinCounter = 0;

    if (LMIC.devaddr != 0)
        return true;

    if (joinCounter >= repeat) {
        log_msg("Canceling LoRaWAN join, already tried %d times in this session!", repeat);
        lmic_status = NOTJOINED;
        joinCounter = 0;
        return false;
    }

    for (joinCounter = 0; (joinCounter < repeat && LMIC.devaddr == 0); joinCounter++) {
        log_msg("Joining network...");
        LMIC_startJoining();
        lmic_pending_jobs(20);  // wait for join to complete
    }

    return (LMIC.devaddr != 0);
}


static void lmic_txdata(osjob_t* j) {
    uint8_t i = 1, rc = 0;
    static uint8_t payload[48];
    static char buf[48];
    uint16_t payloadValInt;
#ifdef LORAWAN_NETWORKTIME
    uint32_t networkTimeEpoch;
#endif

    if (LMIC.opmode & OP_TXRXPEND) {
        log_msg("LMIC is busy, remove scheduled TX job!");
        lmic_remove(j);
        return;
    } else {
        memset(buf, 0, sizeof(buf));
        sprintf(buf, "Preparing LoRaWAN packet %ld", LMIC.seqnoUp+1);

        // request time from LoRaWAN gateway using MAC command DeviceTimeReq
#ifdef LORAWAN_NETWORKTIME
        if (LMIC.seqnoUp % 30 == 0) {
            LMIC_requestNetworkTime(networkTimeCallback, &networkTimeEpoch);
            strcat(buf, " (with network time request)");
        }
#endif
        log_msg(buf);

        memset(payload, 0, sizeof(payload));
        payload[0] = LORAWAN_PAYLOAD_VERSION; // 1

        // sensor sensors status
        payload[i++] = sensorReadings.status; // 2

#ifdef VBAT_PIN
        if (sensorReadings.vbat > 2.55) {
            payload[i++] = 0x01;  // V
            payload[i++] = byte((int)(sensorReadings.vbat * 100) - 256);
        } // 4
#endif
        if ((sensorReadings.status & SENSORS_I2C_FAILED) == 0) {
            payloadValInt = (int)(sensorReadings.temperature * 100);
            payload[i++] = 0x10; // degree celcius
            payload[i++] = (payloadValInt >> 8) & 0xff;
            payload[i++] = payloadValInt & 0xff;
            payload[i++] = 0x11; // %
            payload[i++] = sensorReadings.humidity;
        } // 9

        if (sensorReadings.status & SENSORS_HAS_BME280) {
            payloadValInt = (int)(sensorReadings.pressure * 10);
            payload[i++] = 0x12; // hPa
            payload[i++] = (payloadValInt >> 8) & 0xff;
            payload[i++] = payloadValInt & 0xff;
        } // 12

        if ((sensorReadings.status & SENSORS_SDS011_ERROR) == 0) {
            payloadValInt = (int)(sensorReadings.pm25 * 10);
            payload[i++] = 0x50; // μg/m3
            payload[i++] = (payloadValInt >> 8) & 0xff;
            payload[i++] = payloadValInt & 0xff;
            payloadValInt = (int)(sensorReadings.pm10 * 10);
            payload[i++] = 0x51; // μg/m3
            payload[i++] = (payloadValInt >> 8) & 0xff;
            payload[i++] = payloadValInt & 0xff;
        } // 18

        // queue payload for transmission
        blink_led(250, 1);
        delay(500);
        rc = LMIC_setTxData2(1, payload, i, 0); // port 0
        lmic_remove(j);
        if (rc != LMIC_ERROR_SUCCESS) {
            blink_led(100, 4);
            log_msg("LoRaWAN TX failed with error %d!", rc);
        }
    }
}


// returns given datarate as string (SF7-SF12)
static void dr2str(uint8_t dr, char *drstr) {
    switch (dr) {
        case DR_SF12: strcpy(drstr, "SF12"); break;
        case DR_SF11: strcpy(drstr, "SF11"); break;
        case DR_SF10: strcpy(drstr, "SF10"); break;
        case DR_SF9: strcpy(drstr, "SF9"); break;
        case DR_SF8: strcpy(drstr, "SF8"); break;
        case DR_SF7: strcpy(drstr, "SF7"); break;
        case DR_SF7B: strcpy(drstr, "SF7B"); break;
        case DR_FSK: strcpy(drstr, "FSK"); break;
        default: strcpy(drstr, "ERR"); break;
    }
}


// returns string with req, datarate, payload size, OTAA/ABP,
// ADR status and ACK settings on current LoRaWAN transmission
static char* lmic_txinfo() {
    static char txinfo[48];
    char buf[8];

    memset(txinfo, 0, sizeof(txinfo));
    if (LMIC.seqnoUp == 0)
        sprintf(txinfo, "tx,join,");
    else
        sprintf(txinfo, "tx,%ld,", LMIC.seqnoUp);
    dtostrf(LMIC.freq / 1000000.0, 5, 1, buf);
    strcat(txinfo, buf);
    strcat(txinfo,",");
    itoa(LMIC.pendTxPort, buf, 10);  // port
    strcat(txinfo, buf);
    strcat(txinfo, ",");
    itoa(LMIC.dataLen, buf, 10); // total frame length
    strcat(txinfo, buf);
    strcat(txinfo, ",");
    itoa(LMIC.pendTxLen, buf, 10); // length tx data (payload)
    strcat(txinfo, LMIC.pendTxLen < LMIC.dataLen ? buf : "-");
    strcat(txinfo, ",");
    dr2str(LMIC.datarate, buf);
    strcat(txinfo, buf);
    strcat(txinfo, ",");
    itoa(LMIC.adrTxPow, buf, 10);
    strcat(txinfo, buf);
  
#ifdef LORAWAN_ADR
    strcat(txinfo, ",adr");
#else
    strcat(txinfo, ",noadr");
#endif
    return txinfo;
}


// returns string with freq, datarate, payload size of last LoRaWAN reception
static char* lmic_rxinfo() {
    static char rxinfo[48];
    char buf[8];

    memset(rxinfo, 0, sizeof(rxinfo));
    sprintf(rxinfo, "rx%d,%ld,", (LMIC.txrxFlags & TXRX_DNW1) ? 1 : 2, LMIC.seqnoDn);
    dtostrf(LMIC.freq / 1000000.0, 5, 1, buf);
    strcat(rxinfo, buf);
    strcat(rxinfo,",");
    itoa(LMIC.frame[LMIC.dataBeg-1], buf, 10); // port (if present)
    strcat(rxinfo, (LMIC.txrxFlags & TXRX_PORT) != 0 ? buf : "-");
    strcat(rxinfo, ",");
    itoa(LMIC.dataBeg+LMIC.dataLen, buf, 10); // total frame length
    strcat(rxinfo, buf);
    strcat(rxinfo, ",");
    itoa(LMIC.dataLen, buf, 10); // size of payload (or mac command)
    strcat(rxinfo, (LMIC.dataLen > 0) ? buf : "-");
    strcat(rxinfo, ",");
    dr2str(LMIC.datarate, buf);
    strcat(rxinfo, buf);
    strcat(rxinfo, ",");
    itoa(LMIC.rssi - RSSI_OFF, buf, 10);
    strcat(rxinfo, buf);
    strcat(rxinfo, ",");
    itoa((LMIC.snr+2)/4, buf, 10);  // LMIC.snr is SNR times 4
    strcat(rxinfo, buf);
    return rxinfo;
}


// NOTE: empty function call os_getBattLevel() needs to be removed
// or commented out in src/lmic/lmic.c to avoid 'multiple definition'
// compiler errors before setting this option!
uint8_t os_getBattLevel() {
#if defined(LORAWAN_MAC_BATLEVEL) && defined(VBAT_PIN)
    char buf[8];
    double vbat = (analogRead(VBAT_PIN) * VBAT_MULTIPLIER * 3.3) / 1024;
    if (vbat < VBAT_MIN_LEVEL) {
        Serial.println("1)");
        return 1;
    } else if (vbat > VBAT_MAX_LEVEL) {
        Serial.println("0)");
        return 0;
    } else if (vbat >= VBAT_MIN_LEVEL && vbat <= VBAT_MAX_LEVEL) {
        dtostrf(vbat, 4, 2, buf);
        // remap Li-Ion battery voltage to 1-254
        uint8_t batLevel = (uint8_t) mapfloat(vbat,
            VBAT_MIN_LEVEL, VBAT_MAX_LEVEL, MCMD_DEVS_BATT_MIN, MCMD_DEVS_BATT_MAX);
        log_msg("LNS requesting battery level (%sV -> %d", buf, batLevel);    
        return batLevel;
    } else
#endif
    return 255;
}


// process LMIC events
void onEvent (ev_t ev) {
    static uint32_t txStartMillis = 0;

    switch(ev) {
        case EV_JOINING:
            log_msg("Start joining network...");
            print_otaa_data();
            break;
        case EV_JOINED:
            log_msg("Successfully joined network (%ld ms, RSSI: %d dbm, SNR: %d db)",
                (millis() - txStartMillis), (LMIC.rssi - RSSI_OFF), ((LMIC.snr+2)/4));
            txStartMillis = 0;
            print_session_keys();
            blink_led(200, 2);
#ifndef LORAWAN_ADR
            LMIC_setAdrMode(0);
            log_msg("ADR disabled");
#endif
#ifndef LORAWAN_LINK_CHECK
            // Disable link check validation (automatically enabled during join)
            // https://forum.mcci.io/t/lmic-setlinkcheckmode-questions/96
            // might eventually lead to EV_LINK_DEAD if there a no frequent DL messages
            LMIC_setLinkCheckMode(0);
            log_msg("LinkCheckMode disabled");
#endif
            lmic_status = JOINED;
            break;
        case EV_JOIN_FAILED:
            log_msg("EV_JOIN_FAILED");
            lmic_status = NOTJOINED;
            blink_led(100, 5);
            break;
        case EV_REJOIN_FAILED:
            log_msg("EV_REJOIN_FAILED");
            lmic_status = NOTJOINED;
            blink_led(100, 5);
            break;
        case EV_TXCOMPLETE:
            if ((LMIC.txrxFlags & (TXRX_DNW1|TXRX_DNW2)) != 0) {
                log_msg("TX/RX completed (%ld ms, RSSI: %d dbm, SNR: %d db)",
                    (millis() - txStartMillis), LMIC.rssi - RSSI_OFF), ((LMIC.snr+2)/4);
            } else {
                log_msg("TX/RX completed (%ld ms)", (millis() - txStartMillis));
            }

            if ((LMIC.txrxFlags & (TXRX_DNW1|TXRX_DNW2)) != 0) {
                if ((LMIC.txrxFlags & TXRX_ACK) != 0 && LMIC.dataBeg <= 8)
                    log_msg("Received ACK (%s)", lmic_rxinfo());
                else if ((LMIC.txrxFlags & TXRX_NOPORT) != 0)
                    log_msg("Received MAC command (%s)", lmic_rxinfo());
                else
                    log_msg("Received downlink message (%s)", lmic_rxinfo());
                blink_led(50, 4);
            } else {
                blink_led(50, 2);
            }
            LMIC_clrTxData();
            // Only switch to status TXDONE if sensor data has actually
            // been queued for transmission with lmic_send().
            // This avoids going to sleep to early after an intermittent
            // LoRaWAN transmissions which can be triggered by LoRaWAN MAC
            // commands from the network server (eg. battery status requests)
            if (lmic_status == TXPENDING)
                lmic_status = TXDONE;
            break;
        case EV_RESET:
            log_msg("EV_RESET");
            break;
        case EV_RXCOMPLETE:
            log_msg("EV_RXCOMPLETE");
            break;
        case EV_LINK_DEAD:
            log_msg("EV_LINK_DEAD");
            lmic_status = ERROR;
            blink_led(100, 10);
            break;
        case EV_LINK_ALIVE:
            log_msg("EV_LINK_ALIVE");
            lmic_status = IDLE;
            break;
        case EV_TXSTART:
            log_msg("TX started (%s)%s", lmic_txinfo(),
                (LMIC.devaddr == 0 ? ", waiting for join to complete..." : ""));
            txStartMillis = millis();
            break;
        case EV_JOIN_TXCOMPLETE:
            log_msg("Join not accepted!");
            lmic_status = NOTJOINED;
            blink_led(100, 5);
            break;
        default:
            log_msg("Oops, unknown event: %d", (unsigned)ev);
            break;
    }
}


// initialize LMIC library
void lmic_init() {
    log_msg("Init MCCI LoRaWAN LMIC Library %s", lmic_version());
    os_init();

    // resets the MAC state
    // session and pending data transfers will be discarded
    LMIC_reset();
    LMIC_setClockError(MAX_CLOCK_ERROR * 2 / 100);
    lmic_status = IDLE;
}


// schedule job to transmit observation data
void lmic_send() {
    if (lmic_status == ERROR || lmic_status == IDLE) // try to recover with LMIC reset
        lmic_init();

    // observation data already scheduled?
    if (os_jobIsTimed(&observMsg))
        return;

    if (lmic_join(1)) {
        log_msg("Scheduling observation data");
        os_setTimedCallback(&observMsg, os_getTime() + ms2osticks(500), lmic_txdata);
        lmic_status = TXPENDING;
        blink_led(50, 1);
    } else {
        log_msg("Skipping LoRaWAN TX, not joined!");
    }
}


// prepare LMIC stack for sleep state
void lmic_clear() {
    lmic_remove(&observMsg);
    lmic_status = JOINED;
}
