#ifndef CONFIG_H
#define CONFIG_H

#include <Keyboard.h>  // Librería estándar en lugar de USBComposite

// ============= DEFINIR CÓDIGOS DE TECLAS =============
// El core oficial usa diferentes constantes
#define KEY_F1  0xC2
#define KEY_F2  0xC3
#define KEY_F3  0xC4
#define KEY_F4  0xC5
#define KEY_F5  0xC6
#define KEY_F6  0xC7
#define KEY_F7  0xC8
#define KEY_F8  0xC9
#define KEY_F9  0xCA
#define KEY_F10 0xCB
#define KEY_F11 0xCC
#define KEY_F12 0xCD

#define KEY_LEFT_ARROW  0xD8
#define KEY_RIGHT_ARROW 0xD7
#define KEY_UP_ARROW    0xDA
#define KEY_DOWN_ARROW  0xD9
#define KEY_BACKSPACE   0xB2
#define KEY_TAB         0xB3
#define KEY_RETURN      0xB0
#define KEY_ESC         0xB1
#define KEY_PAGE_UP     0xD3
#define KEY_PAGE_DOWN   0xD6

// ============= CONFIGURACIÓN DE TIMING =============
#define MAIN_LOOP_INTERVAL 5
#define I2C_CHECK_INTERVAL 1000
#define DEBUG_PRINT_INTERVAL 5000

// ============= CONFIGURACIÓN DE DEBOUNCE =============
#define DEBOUNCE_SIMPLE true
#define BUTTON_DEBOUNCE_DELAY 50
#define ENCODER_DEBOUNCE_DELAY 5
#define DEBOUNCE_SAMPLES 5

// ============= CONFIGURACIÓN PCF8575 =============
#define PCF8575_ADDRESS 0x20
#define PCF8575_MAX_RETRIES 3
#define PCF8575_RETRY_DELAY 10

// ============= CONFIGURACIÓN ENCODERS =============
#define ENCODER_A_PIN1 PA0
#define ENCODER_A_PIN2 PA1
#define ENCODER_B_PIN1 PA2
#define ENCODER_B_PIN2 PA3

// ============= CONFIGURACIÓN USB HID =============
#define USB_POLL_INTERVAL 1
#define KEY_PRESS_DURATION 10
#define KEY_RELEASE_DELAY 5

// ============= CONFIGURACIÓN DE BUFFER =============
#define KEY_BUFFER_SIZE 32
#define BUFFER_OVERFLOW_THRESHOLD 24
#define COMBO_TIMEOUT 500
#define MAX_COMBO_LENGTH 8

// ============= CONFIGURACIÓN MODO CONFIG =============
#define CONFIG_MODE_ENABLED true
#define CONFIG_ENTRY_KEYS {0, 11}
#define CONFIG_HOLD_TIME 3000
#define CONFIG_TIMEOUT 10000

// ============= MAPEO DE TECLAS =============
struct KeyMap {
  uint8_t port;
  uint8_t pin;
  uint8_t keycode;
  const char description[10];
};

KeyMap BUTTON_MAP[16] = {
  {0, 0, KEY_F1,  "F1"},
  {0, 1, KEY_F2,  "F2"},
  {0, 2, KEY_F3,  "F3"},
  {0, 3, KEY_F4,  "F4"},
  {0, 4, KEY_F5,  "F5"},
  {0, 5, KEY_F6,  "F6"},
  {0, 6, KEY_F7,  "F7"},
  {0, 7, KEY_F8,  "F8"},
  {1, 0, KEY_F9,  "F9"},
  {1, 1, KEY_F10, "F10"},
  {1, 2, KEY_F11, "F11"},
  {1, 3, KEY_F12, "F12"},
  {1, 4, 'a', "a"},
  {1, 5, 'b', "b"},
  {1, 6, 'c', "c"},
  {1, 7, 'd', "d"}
};

struct EncoderMap {
  uint8_t left_key;
  uint8_t right_key;
  const char description[20];
};

EncoderMap ENCODER_MAP[2] = {
  {'c', 'v', "Encoder A"},
  {'b', 'n', "Encoder B"}
};

// ============= ARRAYS DE TECLAS DISPONIBLES =============
const uint8_t AVAILABLE_LETTERS[] = {
  'a','b','c','d','e','f','g','h','i','j','k','l','m',
  'n','o','p','q','r','s','t','u','v','w','x','y','z'
};

const uint8_t AVAILABLE_NUMBERS[] = {
  '0','1','2','3','4','5','6','7','8','9'
};

const uint8_t AVAILABLE_FUNCTIONS[] = {
  KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
  KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12
};

const uint8_t AVAILABLE_SPECIAL[] = {
  ' ', KEY_RETURN, KEY_TAB, KEY_ESC, KEY_BACKSPACE,
  KEY_LEFT_ARROW, KEY_RIGHT_ARROW, KEY_UP_ARROW, KEY_DOWN_ARROW
};

const uint8_t AVAILABLE_SYMBOLS[] = {
  '!','@','#','$','%','^','&','*','(',')','-','=',
  '[',']','{','}','\\','|',';','\'',',','.','/'
};

// ============= PRIORIDADES DE TECLAS =============
enum KeyPriority {
  PRIORITY_CRITICAL = 0,
  PRIORITY_HIGH = 1,
  PRIORITY_NORMAL = 5,
  PRIORITY_LOW = 9
};

uint8_t getKeyPriority(uint8_t keycode) {
  if(keycode >= KEY_F1 && keycode <= KEY_F12) {
    return PRIORITY_HIGH;
  }
  if(keycode == KEY_ESC || keycode == KEY_RETURN) {
    return PRIORITY_HIGH;
  }
  if((keycode >= 'a' && keycode <= 'z') ||
    (keycode >= '0' && keycode <= '9')) {
    return PRIORITY_NORMAL;
    }
    return PRIORITY_LOW;
}

// ============= VALIDACIÓN Y DEBUG =============
#define DEBUG_MODE true
#define SERIAL_BAUD 115200

#if DEBUG_MODE
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

// ============= ESTADÍSTICAS DEL SISTEMA =============
struct SystemStats {
  unsigned long startTime;
  unsigned long keyPresses;
  unsigned long encoderEvents;
  unsigned long configModeEntries;
  unsigned long i2cErrors;
  unsigned long bufferOverflows;
  unsigned long longestLoopTime;
};

extern SystemStats systemStats;

#endif
