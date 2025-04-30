// File: main.c
// STM32L432KC — UART‐decode 0xA5/0x5A + 4‐byte JoystickData
// Isaac Medina/Jan Konings/Ryan Chen — 4/29/2025

// Include files
#include "ee14lib.h"
#include "stm32l432xx.h"
#include <stdio.h>

// Pin definitions for output to motors:
// Servo pin
#define SERVO_PWM_PIN D1 // PA9 - TIM1 CH2 

// H-bridge pins
#define AIN1 D4 
#define AIN2 D5 
#define PWMA A1 //TIM2
#define STANDBY D6

// Same struct as on the ESP32 
typedef struct __attribute__((packed)) {
    uint16_t joystickX;
    uint16_t joystickY;
} JoystickData;

// set_servo_angle
// Input: The angle in degrees to turn the servo to
// Output: Sets the angle of the servo motor based on a PWM signal
void servo_set_angle(uint16_t JoystickX)
{
    unsigned int degrees = (JoystickX * 180) / 4095;
    if (degrees > 180) degrees = 180; // limit to max
    
    // Widen duty range to match servo
    unsigned int duty_min = (65535 * 5) / 100; 
    unsigned int duty_max = (65535 * 15) / 100;

    // Linear interpolation
    unsigned int duty = duty_min + ((duty_max - duty_min) * degrees) / 180;
    timer_config_channel_pwm(TIM1, SERVO_PWM_PIN, duty); 
}

// Motor direction functions
void set_motor_direction_forward(void)
{
    gpio_write(AIN1, 1);
    gpio_write(AIN2, 0);
}

void set_motor_direction_reverse(void)
{
    gpio_write(AIN1, 0);
    gpio_write(AIN2, 1);
}

void set_motor_stop(void)
{
    gpio_write(AIN1, 0);
    gpio_write(AIN2, 0);
}

// set_motor_speed
// Input: The wirelessly transmitted joystick y vaue
// Output: Sets the pwm of to the PWMA input of the H-bridge
void set_motor_speed(uint16_t Joystick_Y){
  char dbg[64];
  // Define the stop range
  if (Joystick_Y >= 1995 && Joystick_Y <= 2025){
    set_motor_stop();
  }

  else if (Joystick_Y > 2025){
    set_motor_direction_forward();
    unsigned int duty_forward = ((Joystick_Y - 2025) * 65535) / (4095 - 2025);
    
    if (duty_forward > 50000){ // Limit PWM to not overload the motor
      duty_forward = 50000;
    } // 2025–4095 mapped to 0–65535
    timer_config_channel_pwm(TIM2, PWMA, duty_forward);
  }
  else if (Joystick_Y < 1995){
    set_motor_direction_reverse();
    unsigned int duty_reverse = (((1995 - (Joystick_Y+1)) * 65535) / 1995);
    if (duty_reverse > 50000){
      duty_reverse = 50000;
    }
    timer_config_channel_pwm(TIM2, PWMA, duty_reverse);
  }
}

int main(void)
{
    host_serial_init();           // Sets up USART2 on PA2=TX / PA1=RX
    SysTick_initialize();         // Initialize SysTick
    timer_config_pwm(TIM2, 1000); // Set TIM2 to 1000Hz for the DC motor
    timer_config_pwm(TIM1, 50);   // Set TIM1 for PWM at 50Hz for the servo

    // Configure AIN1 and AIN2 as digital outputs
    gpio_config_mode(AIN1, OUTPUT);
    gpio_config_mode(AIN2, OUTPUT);
    gpio_config_mode(STANDBY, OUTPUT);
    gpio_write(STANDBY, 1);        // Bring H-bridge out of standby mode

    // State machine states
    enum {
        WAIT_SYNC1,
        WAIT_SYNC2,
        RECV_X_L,  // low byte of joystickX
        RECV_X_H,  // high byte of joystickX
        RECV_Y_L,  // low byte of joystickY
        RECV_Y_H   // high byte of joystickY
    } state = WAIT_SYNC1;

    JoystickData js;                   // where we assemble the 4 payload bytes
    uint8_t *p = (uint8_t*)&js;        // byte‐pointer into that struct
    int idx = 0;                       // how many payload bytes we’ve got so far
    char dbg[64];                      // for debug prints

    // Main loop
    while (1) {
        // 1) Drain all waiting bytes
        while (USART2->ISR & USART_ISR_RXNE) {
            uint8_t b = USART2->RDR & 0xFF;  // read (and clear RXNE)
            int L;

            // 2) Advance the state machine
            switch(state) {
                case WAIT_SYNC1:
                  if (b == 0xA5) state = WAIT_SYNC2;
                  break;
              
                case WAIT_SYNC2:
                  if (b == 0x5A) {
                    state = RECV_X_L;
                  } else {
                    state = WAIT_SYNC1;
                  }
                  break;
              
                case RECV_X_L:
                  js.joystickX  = b;       // LSB
                  state         = RECV_X_H;
                  break;
              
                case RECV_X_H:
                  js.joystickX |= (b << 8); // MSB
                  state         = RECV_Y_L;
                  break;
              
                case RECV_Y_L:
                  js.joystickY  = b;       // LSB
                  state         = RECV_Y_H;
                  break;
              
                case RECV_Y_H:
                  js.joystickY |= (b << 8); // MSB
                  set_motor_speed(js.joystickY); // Set the motor speed with the aqcuired data
                  servo_set_angle(js.joystickX); // Set the servo angle with the aqcuired data
                  state = WAIT_SYNC1;            // Go look for next packet
                  break;
            }
        }
    }
}