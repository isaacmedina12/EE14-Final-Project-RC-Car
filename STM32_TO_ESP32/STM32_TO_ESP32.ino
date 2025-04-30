// STM32_TO_ESP32
// Made by Isaac Medina, Jan Konings, and Ryan Chen
// Date: 4/28/2025
// ESP-WROOM-32 UART receiver → USB Serial

#include <WiFi.h>
#include <esp_now.h>

// pick two free GPIOs; here we use Serial2 on pins 16 (RX2) and 17 (TX2):
#define STM32_RX_PIN 16   // ESP32 RX2 ← STM32 TX
#define STM32_TX_PIN 17   // not used, but must be specified

struct JoystickData {
  uint16_t joystickX;
  uint16_t joystickY;
};

uint8_t receiverAddress[] = {0xF0, 0xF5, 0xBD, 0x8F, 0xF9, 0x20}; // Receiving ESP32 MAC address
enum RecvState { WAIT1, WAIT2, DATA } state = WAIT1; // State machine states to grab data
uint8_t buf[sizeof(JoystickData)]; // Buffer for getting data
int idx = 0; // Set the starting index to 0

// ESP-NOW send status callback
void onSend(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("ESP-NOW send: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

// Setup
void setup() {
  Serial.begin(115200);                                          // USB serial for debug
  Serial2.begin(115200, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN); // Sset up UART2 for recieving the data from STM32
  Serial.println("ESP32 UART2 receiver ready");                  // Print to make sure it is good to go

  // WiFi in station mode for ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error: ESP-NOW init failed");
    while (1);
  }
  esp_now_register_send_cb(onSend);

  // Add peer
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, receiverAddress, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("Error: Failed to add peer"); // Debug print
    while (1);
  }

  Serial.println("ESP-NOW ready, waiting for UART packets…"); // Print that we are good to go
}

// Main loop to read data
void loop() {

  // Read incoming bytes from STM32
  while (Serial2.available()) {
    uint8_t b = Serial2.read();

    switch (state) {
      case WAIT1:
        if (b == 0xA5) state = WAIT2; // Confirmation byte 1
        break;

      case WAIT2:
        if (b == 0x5A) {              // Confirmation byte 2
          state = DATA;
          idx = 0;
        } else {
          state = WAIT1;
        }
        break;

      // Grab the actual joystick
      case DATA:
        buf[idx++] = b;
        if (idx == sizeof(JoystickData)) {

          // Reassemble struct (little‐endian)
          JoystickData js;
          js.joystickX = buf[0] | (buf[1] << 8);
          js.joystickY = buf[2] | (buf[3] << 8);

          // Debug print
          Serial.printf("Got X=%u Y=%u → sending via ESP-NOW\n",
                        js.joystickX, js.joystickY);

          // Send it
          esp_err_t res = esp_now_send(receiverAddress,
                                       (uint8_t*)&js,
                                       sizeof(js));
          if (res != ESP_OK) {
            Serial.printf("ESP-NOW send error: %d\n", res);
          }
          state = WAIT1; // Reset the state machine
        }
        break;
    }
  }
}