#ifndef CONFIG_MODE_H
#define CONFIG_MODE_H

#include "config.h"
#include "storage.h"
#include <USBComposite.h>

// ============= CONSTANTES DE CONFIGURACIÓN =============
#define CONFIG_ENTRY_HOLD_TIME 3000  // 3 segundos para entrar
#define CONFIG_TIMEOUT 10000          // 10 segundos timeout
#define CONFIG_KEY1 0                 // Índice botón F1 en BUTTON_MAP
#define CONFIG_KEY2 11                 // Índice botón F12 en BUTTON_MAP

// ============= TECLAS DISPONIBLES PARA REMAPEO =============
// Encoder A navega por estas
const uint8_t ENCODER_A_OPTIONS[] = {
  'a','b','c','d','e','f','g','h','i','j','k','l','m',
  'n','o','p','q','r','s','t','u','v','w','x','y','z',
  '0','1','2','3','4','5','6','7','8','9',
  ' ', KEY_RETURN, KEY_TAB, KEY_ESC, KEY_BACKSPACE
};
const uint8_t ENCODER_A_COUNT = sizeof(ENCODER_A_OPTIONS);

// Encoder B navega por estas
const uint8_t ENCODER_B_OPTIONS[] = {
  KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
  KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
  '!','@','#','$','%','^','&','*','(',')','-','=',
  '[',']','{','}','\\','|',';','\'',',','.','/',
  KEY_LEFT_ARROW, KEY_RIGHT_ARROW, KEY_UP_ARROW, KEY_DOWN_ARROW
};
const uint8_t ENCODER_B_COUNT = sizeof(ENCODER_B_OPTIONS);

// ============= CLASE MODO CONFIGURACIÓN =============
class ConfigMode {
private:
  enum State {
    IDLE,
    CHECKING_ENTRY,      // Verificando si se mantienen las teclas
    WAITING_FOR_KEY,     // Esperando que presione tecla a configurar
    SELECTING_NEW_MAP,   // Girando encoders para elegir nuevo mapeo
    CONFIRMING,          // Esperando confirmación
    WAITING_NEXT_ACTION  // Configurar otra o salir
  };
  
  State currentState;
  unsigned long stateStartTime;
  unsigned long configEntryStartTime;
  bool configKeysPressed;
  
  int8_t selectedButton;        // Índice del botón siendo configurado
  int8_t encoderAIndex;         // Posición actual en ENCODER_A_OPTIONS
  int8_t encoderBIndex;         // Posición actual en ENCODER_B_OPTIONS
  uint8_t currentSelection;     // Tecla actualmente seleccionada
  bool usingEncoderB;           // true si última selección fue con encoder B
  
  HIDKeyboard* keyboard;
  
  // Escribir texto en el editor activo
  void typeText(const char* text) {
    while(*text) {
      keyboard->press(*text);
      delay(5);
      keyboard->release(*text);
      text++;
      delay(5);
    }
  }
  
  // Escribir nueva línea
  void typeNewline() {
    keyboard->press(KEY_RETURN);
    delay(5);
    keyboard->release(KEY_RETURN);
    delay(5);
  }
  
  // Borrar línea anterior (para actualizar preview)
  void clearLine() {
    // Ir al inicio de línea y borrar
    for(int i = 0; i < 50; i++) {
      keyboard->press(KEY_BACKSPACE);
      delay(5);
      keyboard->release(KEY_BACKSPACE);
    }
  }
  
  // Obtener nombre de tecla para mostrar
  void getKeyName(uint8_t keycode, char* buffer) {
    if(keycode >= 'a' && keycode <= 'z') {
      sprintf(buffer, "%c", keycode);
    } else if(keycode >= '0' && keycode <= '9') {
      sprintf(buffer, "%c", keycode);
    } else if(keycode == ' ') {
      sprintf(buffer, "SPACE");
    } else if(keycode == KEY_RETURN) {
      sprintf(buffer, "ENTER");
    } else if(keycode == KEY_TAB) {
      sprintf(buffer, "TAB");
    } else if(keycode == KEY_ESC) {
      sprintf(buffer, "ESC");
    } else if(keycode == KEY_BACKSPACE) {
      sprintf(buffer, "BACKSPACE");
    } else if(keycode >= KEY_F1 && keycode <= KEY_F12) {
      sprintf(buffer, "F%d", (keycode - KEY_F1) + 1);
    } else if(keycode == KEY_LEFT_ARROW) {
      sprintf(buffer, "LEFT");
    } else if(keycode == KEY_RIGHT_ARROW) {
      sprintf(buffer, "RIGHT");
    } else if(keycode == KEY_UP_ARROW) {
      sprintf(buffer, "UP");
    } else if(keycode == KEY_DOWN_ARROW) {
      sprintf(buffer, "DOWN");
    } else {
      sprintf(buffer, "%c", keycode);
    }
  }
  
public:
  ConfigMode(HIDKeyboard* kb) : keyboard(kb) {
    currentState = IDLE;
    selectedButton = -1;
    encoderAIndex = 0;
    encoderBIndex = 0;
    currentSelection = 0;
    configKeysPressed = false;
    usingEncoderB = false;
  }
  
  // Verificar si debe entrar en modo config
  bool checkEntry(bool* buttonStates) {
    if(currentState != IDLE && currentState != CHECKING_ENTRY) return false;
    
    // Verificar si F1 y F12 están presionadas
    bool keysPressed = buttonStates[CONFIG_KEY1] && buttonStates[CONFIG_KEY2];
    
    if(keysPressed && !configKeysPressed) {
      // Inicio de presión
      configKeysPressed = true;
      configEntryStartTime = millis();
      currentState = CHECKING_ENTRY;
    } else if(!keysPressed && configKeysPressed) {
      // Se soltaron las teclas
      configKeysPressed = false;
      currentState = IDLE;
    } else if(keysPressed && configKeysPressed) {
      // Verificar si pasaron 3 segundos
      if(millis() - configEntryStartTime >= CONFIG_ENTRY_HOLD_TIME) {
        enterConfigMode();
        return true;
      }
    }
    
    return false;
  }
  
  // Entrar al modo configuración
  void enterConfigMode() {
    currentState = WAITING_FOR_KEY;
    stateStartTime = millis();
    selectedButton = -1;
    
    typeNewline();
    typeText("=== CONFIGURANDO TECLADO ===");
    typeNewline();
    typeText("Presione la tecla a configurar...");
    typeNewline();
  }
  
  // Procesar botón presionado
  void processButton(int8_t buttonIndex) {
    if(currentState == WAITING_FOR_KEY || currentState == WAITING_NEXT_ACTION) {
      // Seleccionar botón a configurar
      selectedButton = buttonIndex;
      currentState = SELECTING_NEW_MAP;
      stateStartTime = millis();
      
      // Buscar mapeo actual en las opciones
      uint8_t currentKey = BUTTON_MAP[buttonIndex].keycode;
      encoderAIndex = -1;
      encoderBIndex = -1;
      
      for(int i = 0; i < ENCODER_A_COUNT; i++) {
        if(ENCODER_A_OPTIONS[i] == currentKey) {
          encoderAIndex = i;
          currentSelection = currentKey;
          usingEncoderB = false;
          break;
        }
      }
      
      if(encoderAIndex == -1) {
        for(int i = 0; i < ENCODER_B_COUNT; i++) {
          if(ENCODER_B_OPTIONS[i] == currentKey) {
            encoderBIndex = i;
            currentSelection = currentKey;
            usingEncoderB = true;
            break;
          }
        }
      }
      
      // Si no se encuentra, inicializar en posición 0
      if(encoderAIndex == -1 && encoderBIndex == -1) {
        encoderAIndex = 0;
        currentSelection = ENCODER_A_OPTIONS[0];
        usingEncoderB = false;
      }
      
      char keyName[20];
      getKeyName(currentKey, keyName);
      char buffer[100];
      sprintf(buffer, "Configurando boton %d. Mapeo actual: [%s]", 
              buttonIndex + 1, keyName);
      typeText(buffer);
      typeNewline();
      typeText("Gire encoder A para letras/numeros, B para simbolos/F");
      typeNewline();
      
    } else if(currentState == SELECTING_NEW_MAP && buttonIndex == selectedButton) {
      // Confirmar selección
      confirmMapping();
    }
  }
  
  // Procesar giro de encoder
  void processEncoder(uint8_t encoderNum, int8_t direction) {
    if(currentState == SELECTING_NEW_MAP) {
      char keyName[20];
      
      if(encoderNum == 0) {  // Encoder A
        encoderAIndex += direction;
        if(encoderAIndex < 0) encoderAIndex = ENCODER_A_COUNT - 1;
        if(encoderAIndex >= ENCODER_A_COUNT) encoderAIndex = 0;
        
        currentSelection = ENCODER_A_OPTIONS[encoderAIndex];
        usingEncoderB = false;
        
      } else {  // Encoder B
        encoderBIndex += direction;
        if(encoderBIndex < 0) encoderBIndex = ENCODER_B_COUNT - 1;
        if(encoderBIndex >= ENCODER_B_COUNT) encoderBIndex = 0;
        
        currentSelection = ENCODER_B_OPTIONS[encoderBIndex];
        usingEncoderB = true;
      }
      
      // Actualizar preview
      clearLine();
      getKeyName(currentSelection, keyName);
      char buffer[50];
      sprintf(buffer, "Nuevo mapeo: [%s]", keyName);
      typeText(buffer);
      
    } else if(currentState == WAITING_NEXT_ACTION) {
      // Girar encoder = salir
      exitConfigMode();
    }
  }
  
  // Confirmar mapeo
  void confirmMapping() {
    // Actualizar mapeo en memoria
    BUTTON_MAP[selectedButton].keycode = currentSelection;
    
    // Guardar en EEPROM
    saveConfiguration();
    
    char keyName[20];
    getKeyName(currentSelection, keyName);
    
    typeNewline();
    char buffer[50];
    sprintf(buffer, "✓ Boton %d configurado como [%s]", 
            selectedButton + 1, keyName);
    typeText(buffer);
    typeNewline();
    typeText("¿Configurar otra tecla? Presionela para comenzar o gire encoder para salir");
    typeNewline();
    
    currentState = WAITING_NEXT_ACTION;
    stateStartTime = millis();
  }
  
  // Salir del modo configuración
  void exitConfigMode() {
    typeText("=== CONFIGURACION GUARDADA ===");
    typeNewline();
    typeNewline();
    
    currentState = IDLE;
    selectedButton = -1;
  }
  
  // Verificar timeout
  void checkTimeout() {
    if(currentState != IDLE && currentState != CHECKING_ENTRY) {
      if(millis() - stateStartTime >= CONFIG_TIMEOUT) {
        typeNewline();
        typeText("Timeout - Saliendo del modo configuracion");
        exitConfigMode();
      }
    }
  }
  
  // Estado actual
  bool isActive() {
    return currentState != IDLE;
  }
  
  State getState() {
    return currentState;
  }
};

#endif