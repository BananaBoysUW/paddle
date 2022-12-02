#include <ESP8266WiFi.h>
#include <espnow.h>
#include <pb.h>
#include "include/proto/pb/pb.h"
#include "include/proto/pb/pb_decode.h"
#include "include/proto/pb/pb_encode.h"
#include "include/proto/pb/pb_common.h"
#include "include/proto/paddle.pb.c"

#define READ_BUF_LEN 256

const size_t addrLen = 6;
uint8_t broadcastAddr[addrLen] = {0x8C, 0x4B, 0x14, 0x5B, 0x15, 0xFC};

void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
    // Serially write the hex encoded data
    for (int i = 0; i < len; i++) {
        Serial.printf("%02X", *(incomingData + i));
    }
    Serial.print('\n');
}

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

    // Add peer
    if (esp_now_add_peer(broadcastAddr, ESP_NOW_ROLE_COMBO, 1, NULL, 0) != 0) {
        Serial.println("Failed to add peer");
        return;
    }

    esp_now_register_recv_cb(OnDataRecv);
}


uint8_t readBuf[READ_BUF_LEN];
void loop() {
    if (Serial.available() == 0) {
        return;
    }

    int len = Serial.readBytesUntil('\n', readBuf, READ_BUF_LEN - 1);
    if (len == 0) {
        return;
    }
    // Trim carriage return
    if (readBuf[len - 1] == '\r') {
        len--;
    }
    // Forward data
    uint8_t result = esp_now_send(broadcastAddr, readBuf, len);
    if (result != 0) {
        Serial.println("Error sending data");
    }
}

