/*

*/

#include <ESP8266WiFi.h>
#include <espnow.h>
#include <pb.h>
#include "include/proto/pb.h"
#include "include/proto/pb_decode.h"
#include "include/proto/pb_encode.h"
#include "include/proto/pb_common.h"
#include "include/proto/paddle.pb.c"

#define READ_BUF_LEN 256

// Callback when data is sent
void OnDataSent(uint8_t *mac_addr, uint8_t status) {
    /* Serial.print("\r\nLast Packet Send Status:\t"); */
    /* Serial.println(status == 0 ? "Delivery Success" : "Delivery Fail"); */
}

// Callback when data is received
/* uint8_t msg[255]; */
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
    /* Serial.print("Bytes received: "); */
    /* Serial.println(len); */
    for (int i = 0; i < len; i++) {
        /* Serial.print(*(incomingData + i)); */
        Serial.printf("%02X", *(incomingData + i));
    }
    Serial.print('\n');
    /* memcpy(msg, incomingData, len); */
    /* msg[len] = '\0'; */
    /* Serial.printf("%s\n", msg); */
}

const size_t addrLen = 6;
uint8_t broadcastAddr[addrLen] = {0x8C, 0x4B, 0x14, 0x5B, 0x15, 0xFC};

const int motor1 = 14;
const int motor2 = 12;
bool motor = false;

void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Initialize ESP-NOW
    if (esp_now_init() != 0) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);

    // Register the send callback
    esp_now_register_send_cb(OnDataSent);

    // Add peer
    if (esp_now_add_peer(broadcastAddr, ESP_NOW_ROLE_COMBO, 1, NULL, 0) != 0) {
        Serial.println("Failed to add peer");
        return;
    }

    // Register the receive callback
    esp_now_register_recv_cb(OnDataRecv);

    pinMode(motor1, OUTPUT);
}


uint8_t readBuf[READ_BUF_LEN];
void loop() {
    if (Serial.available() > 0) {
        int len = Serial.readBytesUntil('\n', readBuf, READ_BUF_LEN - 1);
        if (len > 0 && readBuf[len - 1] == '\r') {
            len--;
        }
        // readBuf[len] = '\0';

        if (len > 0) {
            uint8_t result = esp_now_send(broadcastAddr, readBuf, len);
            if (result == 0) {
                Serial.println("Sent successfully");
            }
            else {
                Serial.println("Error sending data");
            }
        }
        motor = !motor;
        digitalWrite(motor1, motor);
        digitalWrite(motor2, motor);
    }

    /* char *data = "hi there from esp8266"; */
    /* uint8_t result = esp_now_send(broadcastAddr, (uint8_t *)data, strlen(data)); */
    /* if (result == 0) { */
        /* Serial.println("Sent successfully"); */
    /* } */
    /* else { */
        /* Serial.println("Error sending data"); */
    /* } */
}

