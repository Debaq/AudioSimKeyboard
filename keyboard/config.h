#ifndef CONFIG_H
#define CONFIG_H

#include <USBComposite.h>

// ============= CONFIGURACIÓN DE TIMING =============
// Timing no bloqueante
#define MAIN_LOOP_INTERVAL 5      // ms entre ciclos principales
#define I2C_CHECK_INTERVAL 1000    // ms entre verificaciones I2C
#define DEBUG_PRINT_INTERVAL 5000  // ms entre prints de debug

// ============= CONFIGURACIÓN DE DEBOUNCE =============
#define DEBOUNCE_SIMPLE true       // true = simple delay, false = algoritmo sofisticado

// Timing diferenciado para botones y encoders
#define BUTTON_DEBOUNCE_DELAY 50  // ms para botones (más conservador)
#define ENCODER_DEBOUNCE_DELAY 5  // ms para encoders (más rápido)
#define DEBOUNCE_SAMPLES 5         // muestras para debounce sofisticado

// ============= CONFIGURACIÓN PCF8575 =============
#define PCF8575_ADDRESS 0x20       // Dirección I2C por defecto
#define PCF8575_MAX_RETRIES 3      // Reintentos en caso de error
#define PCF8575_RETRY_DELAY 10     // ms entre reintentos

// ============= CONFIGURACIÓN ENCODERS =============
// Pines para Encoder A
#define ENCODER_A_PIN1 PA0
#define ENCODER_A_PIN2 PA1

// Pines para Encoder B  
#define ENCODER_B_PIN1 PA2
#define ENCODER_B_PIN2 PA3

// ============= CONFIGURACIÓN USB HID =============
#define USB_POLL_INTERVAL 1        // ms - polling rate USB
#define KEY_PRESS_DURATION 10      // ms - duración de pulsación
#define KEY_RELEASE_DELAY 5        // ms - delay entre teclas

// ============= CONFIGURACIÓN DE BUFFER =============
#define KEY_BUFFER_SIZE 32         // Tamaño del buffer circular
#define BUFFER_OVERFLOW_THRESHOLD 24  // Avisar cuando llegue a este nivel
#define COMBO_TIMEOUT 500          // ms para detectar combos
#define MAX_COMBO_LENGTH 8         // Máximo teclas en un combo

// ============= CONFIGURACIÓN MODO CONFIG =============
#define CONFIG_MODE_ENABLED true   // Habilitar modo configuración
#define CONFIG_ENTRY_KEYS {0, 11}  // Índices de F1 y F12
#define CONFIG_HOLD_TIME 3000      // ms para entrar al modo
#define CONFIG_TIMEOUT 10000       // ms timeout de inactividad

// ============= MAPEO DE TECLAS =============
struct KeyMap {
  uint8_t port;              // 0 o 1 (P0 o P1 del PCF8575)
  uint8_t pin;               // 0-7 (bit dentro del puerto)
  uint8_t keycode;           // Código HID de la tecla (modificable)
  const char description[10]; // Descripción para debug (fija)
};

// Mapeo de los 16 pines del PCF8575
// NOTA: keycode ahora es modificable en runtime
KeyMap BUTTON_MAP[16] = {
  // Puerto P0 (bits 0-7) -> F1 a F8
  {0, 0, KEY_F1,  "F1"},
  {0, 1, KEY_F2,  "F2"},
  {0, 2, KEY_F3,  "F3"},
  {0, 3, KEY_F4,  "F4"},
  {0, 4, KEY_F5,  "F5"},
  {0, 5, KEY_F6,  "F6"},
  {0, 6, KEY_F7,  "F7"},
  {0, 7, KEY_F8,  "F8"},
  
  // Puerto P1 (bits 0-3) -> F9 a F12
  {1, 0, KEY_F9,  "F9"},
  {1, 1, KEY_F10, "F10"},
  {1, 2, KEY_F11, "F11"},
  {1, 3, KEY_F12, "F12"},
  
  // Puerto P1 (bits 4-7) -> letras a, b, c, d
  {1, 4, 'a', "a"},
  {1, 5, 'b', "b"}, 
  {1, 6, 'c', "c"},
  {1, 7, 'd', "d"}
};

// Mapeo de encoders (también modificable)
struct EncoderMap {
  uint8_t left_key;           // Tecla para giro a la izquierda
  uint8_t right_key;          // Tecla para giro a la derecha
  const char description[20]; // Descripción fija
};

EncoderMap ENCODER_MAP[2] = {
  {'c', 'v', "Encoder A"},    // Encoder A: izq=c, der=v
  {'b', 'n', "Encoder B"}     // Encoder B: izq=b, der=n
};

// ============= ARRAYS DE TECLAS DISPONIBLES =============
// Para el modo configuración - todas las teclas posibles
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
// Para el buffer con prioridad
enum KeyPriority {
  PRIORITY_CRITICAL = 0,    // Teclas de sistema/config
  PRIORITY_HIGH = 1,        // F1-F12
  PRIORITY_NORMAL = 5,      // Letras y números
  PRIORITY_LOW = 9          // Otras teclas
};

// Obtener prioridad de una tecla
uint8_t getKeyPriority(uint8_t keycode) {
  // Teclas de función tienen alta prioridad
  if(keycode >= KEY_F1 && keycode <= KEY_F12) {
    return PRIORITY_HIGH;
  }
  // Teclas especiales
  if(keycode == KEY_ESC || keycode == KEY_RETURN) {
    return PRIORITY_HIGH;
  }
  // Letras y números prioridad normal
  if((keycode >= 'a' && keycode <= 'z') || 
     (keycode >= '0' && keycode <= '9')) {
    return PRIORITY_NORMAL;
  }
  // Todo lo demás baja prioridad
  return PRIORITY_LOW;
}

// ============= VALIDACIÓN Y DEBUG =============
#define DEBUG_MODE true           // Habilitar mensajes debug
#define SERIAL_BAUD 115200        // Velocidad del puerto serie

// Macro para debug condicional
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

// Estadísticas globales
extern SystemStats systemStats;

#endif