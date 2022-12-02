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

