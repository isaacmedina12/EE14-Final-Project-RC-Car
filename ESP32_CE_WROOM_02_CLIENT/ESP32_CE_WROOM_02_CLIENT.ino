// ESP32_CE_WROOM_02_CLIENT

// Include files
#include <WiFi.h>
#include <esp_now.h>

// Joystick data struct
typedef struct __attribute__((packed)) {
  uint16_t joystickX;
  uint16_t joystickY;
} JoystickData;

JoystickData incomingData; // Define the income data variable as a Joystick data struct type

// == ESP-NOW receive callback (IDF style) ==
void OnDataRecv(const esp_now_recv_info_t *info,
                const uint8_t *data,
                int len) {
  // 1) Show sender MAC
  Serial.print("From MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X%s", info->src_addr[i], i < 5 ? ":" : "\n");
  }

  // 2) validate & unpack
  if (len == sizeof(incomingData)) {
    memcpy(&incomingData, data, len);
    Serial.printf("Received X: %u | Y: %u\n",
                  incomingData.joystickX,
                  incomingData.joystickY);
  
    // 3) forward the 4 bytes out UART0 (GPIO20=RX0, GPIO21=TX0)
    Serial1.write((uint8_t)0xA5);
    Serial1.write((uint8_t)0x5A);
    uint8_t* p = (uint8_t*)&incomingData;
    for (size_t i = 0; i < sizeof(incomingData); i++) {
      Serial1.write(p[i]);
    }

  } else {
    Serial.printf("⚠️ Wrong packet size: %d\n", len);
  }
}

void setup() {
  // —— USB-Serial for debug & forwarding over GPIO20/21 ——
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, 2, 3);
  delay(1000);
  Serial.println("ESP-NOW → UART0 bridge starting...");

  // —— Prepare ESP-NOW in station mode ——
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error: ESP-NOW init failed");
    while (true) { delay(1000); }
  }

  // register receive callback
  esp_err_t err = esp_now_register_recv_cb(OnDataRecv);
  if (err != ESP_OK) {
    Serial.printf("Error: esp_now_register_recv_cb failed (%d)\n", err);
    while (true) { delay(1000); }
  }

  Serial.println("ESP-NOW receiver ready — forwarding on UART0 pins 20/21");
}

void loop() {
  // nothing to do here; OnDataRecv does all the work
}
