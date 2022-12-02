#include <WiFi.h>
#include <esp_now.h>
#include <pb.h>
#include "include/proto/pb/pb.h"
#include "include/proto/pb/pb_decode.h"
#include "include/proto/pb/pb_encode.h"
#include "include/proto/pb/pb_common.h"
#include "include/proto/paddle.pb.c"

#include <NewPing.h>

#define TRIGGER_PIN  15  // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define ECHO_PIN     4   // Arduino pin tied to echo pin on the ultrasonic sensor.
#define MAX_DISTANCE 200 // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.

#define NUMMOTORS 4

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);

esp_now_peer_info_t peerInfo;

typedef struct _BuzzState {
    int pin;
    bool isBuzzing = false;
    uint32_t endTime; // check out esp_timer_get_time() instead of millis()
} BuzzState;
BuzzState buzzStates[NUMMOTORS];

typedef struct {
    int32_t numbers[NUMMOTORS];
    int32_t len;
} MotorList;

// Add a number to the int list
void MotorList_add_number(MotorList *list, int32_t number) {
    if (list->len < NUMMOTORS) {
        list->numbers[list->len] = number;
        list->len++;
    }
}

bool PaddleIn_decode_single_number(pb_istream_t *istream, const pb_field_t *field, void **arg) {
    MotorList *dest = (MotorList*)(*arg);

    // decode single number
    uint32_t number;
    if (!pb_decode_varint32(istream, &number)) {
        const char * error = PB_GET_ERROR(istream);
        printf("PaddleIn_decode_single_number error: %s", error);
        return false;
    }

    // add to destination list
    MotorList_add_number(dest, (int32_t)number);
    return true;
}

// https://forum.arduino.cc/t/how-to-turn-a-string-hex-code-into-its-ascii-translation/611618/3
char *unHex(const char* input, char* target, size_t len) {
    if (target != nullptr && len) {
        size_t inLen = strlen(input);
        if (inLen & 1) {
            Serial.println(F("unhex: malformed input"));
        }
        size_t chars = inLen / 2;
        if (chars >= len) {
            Serial.println(F("unhex: target buffer too small"));
            chars = len - 1;
        }
        for (size_t i = 0; i < chars; i++) {
            target[i] = aNibble(*input++);
            target[i] <<= 4;
            target[i] |= aNibble(*input++);
        }
        target[chars] = 0;
    } 
    else {
        Serial.println(F("unhex: no target buffer"));
    }
    return target;
}
byte aNibble(char in) {
    if (in >= '0' && in <= '9') {
        return in - '0';
    } 
    else if (in >= 'a' && in <= 'f') {
        return in - 'a' + 10;
    } 
    else if (in >= 'A' && in <= 'F') {
        return in - 'A' + 10;
    }
    return 0;
}


// Callback when data is received
uint8_t tmpBuf[256];
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    unHex((const char *)incomingData, (char *)tmpBuf, 256);

    MotorList motorList = { 0 };
    brickbreaker_PaddleIn paddleIn = brickbreaker_PaddleIn_init_zero;

    paddleIn.buzz.motors.arg = &motorList;
    paddleIn.buzz.motors.funcs.decode = PaddleIn_decode_single_number;

    // Create a stream that reads from the buffer
    pb_istream_t stream = pb_istream_from_buffer(tmpBuf, strlen((char *)tmpBuf));
    // Deserialize the message
    bool status = pb_decode(&stream, &brickbreaker_PaddleIn_msg, &paddleIn);
    if (!status) {
        Serial.print(F("Decoding failed: "));
        Serial.println(PB_GET_ERROR(&stream));
        return;
    }

    // Handle buzz
    if (paddleIn.has_buzz) {
        for (int i = 0; i < motorList.len; i++) {
            buzzStates[i].endTime = millis() + paddleIn.buzz.durationMillis;
        }
    }
}

const size_t addrLen = 6;
uint8_t broadcastAddr[addrLen] = {0xE8, 0xDB, 0x84, 0x9B, 0x81, 0xF3};

void setup() {
    Serial.begin(115200);
    
    buzzStates[0].pin = 21;
    buzzStates[1].pin = 19;
    buzzStates[2].pin = 18;
    buzzStates[3].pin = 5;

    for (int i = 0; i < NUMMOTORS; i++) {
        pinMode(buzzStates[i].pin, OUTPUT);
        digitalWrite(buzzStates[i].pin, LOW);
    }

    WiFi.mode(WIFI_STA);

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // Prepare peerInfo struct
    memcpy(peerInfo.peer_addr, broadcastAddr, addrLen);
    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }

    // Register the receive callback
    esp_now_register_recv_cb(OnDataRecv);
}

// Non blocking delay setup
uint32_t updateDelay = 50;
uint32_t lastUpdate = 0;

// Serialization setup
double dist;
uint8_t buf[256];
brickbreaker_PaddleOut paddleOut = brickbreaker_PaddleOut_init_zero;

void loop() {
    // Non blocking update delay 
    if (millis() - lastUpdate >= updateDelay) {
        // Read ultrasonic distance sensor data
        dist = (sonar.ping() / 2) * 0.0343;
        paddleOut.distance = dist;
        
        // Serialize data
        pb_ostream_t stream = pb_ostream_from_buffer(buf, sizeof(buf));
        bool status = pb_encode(&stream, &brickbreaker_PaddleOut_msg, &paddleOut);
        if (!status) {
            Serial.print(F("Encoding failed: "));
            Serial.println(PB_GET_ERROR(&stream));
        }
        
        // Broadcast data
        size_t message_length = stream.bytes_written;
        if (message_length > 0) {
            esp_err_t result = esp_now_send(broadcastAddr, (const uint8_t*)buf, message_length);
            if (result != ESP_OK) {
                Serial.println(F("Error sending data"));
            }
        }

        lastUpdate = millis();
    }

    // Update motors
    for (int i = 0; i < NUMMOTORS; i++) {
        if (!buzzStates[i].isBuzzing && buzzStates[i].endTime > millis()) {
            // set digital pin for buzzer to HIGH
            buzzStates[i].isBuzzing = true;
            digitalWrite(buzzStates[i].pin, HIGH);
            Serial.printf("SET motor %d pin %d HIGH\n", i+1, buzzStates[i].pin);
        }
        else if (buzzStates[i].isBuzzing && buzzStates[i].endTime <= millis()) {
            // set digital pin for buzzer to LOW
            buzzStates[i].isBuzzing = false;
            digitalWrite(buzzStates[i].pin, LOW);
            Serial.printf("SET motor %d pin %d LOW\n", i+1, buzzStates[i].pin);
        }
    }
}

