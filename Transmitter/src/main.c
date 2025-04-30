// main.c
// RC Car transmitter side code for EE14 final project
// Made by: Isaac Medina, Jan Konings, and Ryan Chen
// 4/28/2025


// Include header files
#include "ee14lib.h"
#include "stm32l432xx.h"

// ADC inputs
#define VRX_PIN A6 // VRX pin for ADC read from joystick
#define VRY_PIN D3 // VRY pin for ADC read from joystick

// Joystick data struct
typedef struct __attribute__((packed)) {
    uint16_t joystickX;
    uint16_t joystickY;
} JoystickData;

// Main 
int main(void)
{

    // Initialize the serial monitor if we need it
    host_serial_init();
   
    // Initialize SysTick for delay functions
    SysTick_initialize();

    // Define struct variable to make data packets whenever we want
    JoystickData js;
    uint8_t *p = (uint8_t*)&js;

    
    // Main loop
    while (1)
    {
        adc_config_single(VRY_PIN);
        js.joystickX = adc_read_single(); // Pack X data into struct
        
        // Speed and direction of the main motor
        adc_config_single(VRX_PIN);
        js.joystickY = adc_read_single(); // Pack Y data into struct

        // 3) send a 2-byte header so the receiver can sync
        UART_write_byte(USART2, 0xA5);
        UART_write_byte(USART2, 0x5A);

        
        // 4) send the 4 bytes of the struct (little-endian)
        for (int i = 0; i < sizeof(JoystickData); i++) {
            UART_write_byte(USART2, p[i]);
        }

        delay_ms(100); // 10Hz update rate
    }   
}