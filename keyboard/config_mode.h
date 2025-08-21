#ifndef CONFIG_MODE_H
#define CONFIG_MODE_H

#include "config.h"
#include "storage.h"
#include <Keyboard.h>

// ============= CONSTANTES DE CONFIGURACIÓN =============
#define CONFIG_ENTRY_HOLD_TIME 3000
#define CONFIG_TIMEOUT 10000
#define CONFIG_KEY1 0
#define CONFIG_KEY2 11

// ============= TECLAS DISPONIBLES PARA REMAPEO =============
const uint8_t ENCODER_A_OPTIONS[] = {
  'a','b','c','d','e','f','g','h','i','j','k','l','m',
  'n','o','p','q','r','s','t','u','v','w','x','y','z',
  '0','1','2','3','4','5','6','7','8','9',
  ' ', KEY_RETURN, KEY_TAB, KEY_ESC, KEY_BACKSPACE
};
const uint8_t ENCODER_A_COUNT = sizeof(ENCODER_A_OPTIONS);

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
    CHECKING_ENTRY,
    WAITING_FOR_KEY,
    SELECTING_NEW_MAP,
    CONFIRMING,
    WAITING_NEXT_ACTION
  };

  State currentState;
  unsigned long stateStartTime;
  unsigned long configEntryStartTime;
  bool configKeysPressed;

  int8_t selectedButton;
  int8_t encoderAIndex;
  int8_t encoderBIndex;
  uint8_t currentSelection;
  bool usingEncoderB;

  // Escribir texto en el editor activo
  void typeText(const char* text) {
    while(*text) {
      Keyboard.write(*text);
      delay(10);
      text++;
    }
  }

  // Escribir nueva línea
  void typeNewline() {
    Keyboard.write(KEY_RETURN);
    delay(10);
  }

  // Borrar línea anterior
  void clearLine() {
    for(int i = 0; i < 50; i++) {
      Keyboard.write(KEY_BACKSPACE);
      delay(5);
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
  ConfigMode() {
    currentState = IDLE;
    selectedButton = -1;
    encoderAIndex = 0;
    encoderBIndex = 0;
    currentSelection = 0;
    configKeysPressed = false;
    usingEncoderB = false;
  }

  bool checkEntry(bool* buttonStates) {
    if(currentState != IDLE && currentState != CHECKING_ENTRY) return false;

    bool keysPressed = buttonStates[CONFIG_KEY1] && buttonStates[CONFIG_KEY2];

    if(keysPressed && !configKeysPressed) {
      configKeysPressed = true;
      configEntryStartTime = millis();
      currentState = CHECKING_ENTRY;
    } else if(!keysPressed && configKeysPressed) {
      configKeysPressed = false;
      currentState = IDLE;
    } else if(keysPressed && configKeysPressed) {
      if(millis() - configEntryStartTime >= CONFIG_ENTRY_HOLD_TIME) {
        enterConfigMode();
        return true;
      }
    }

    return false;
  }

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

  void processButton(int8_t buttonIndex) {
    if(currentState == WAITING_FOR_KEY || currentState == WAITING_NEXT_ACTION) {
      selectedButton = buttonIndex;
      currentState = SELECTING_NEW_MAP;
      stateStartTime = millis();

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
      confirmMapping();
    }
  }

  void processEncoder(uint8_t encoderNum, int8_t direction) {
    if(currentState == SELECTING_NEW_MAP) {
      char keyName[20];

      if(encoderNum == 0) {
        encoderAIndex += direction;
        if(encoderAIndex < 0) encoderAIndex = ENCODER_A_COUNT - 1;
        if(encoderAIndex >= ENCODER_A_COUNT) encoderAIndex = 0;

        currentSelection = ENCODER_A_OPTIONS[encoderAIndex];
        usingEncoderB = false;

      } else {
        encoderBIndex += direction;
        if(encoderBIndex < 0) encoderBIndex = ENCODER_B_COUNT - 1;
        if(encoderBIndex >= ENCODER_B_COUNT) encoderBIndex = 0;

        currentSelection = ENCODER_B_OPTIONS[encoderBIndex];
        usingEncoderB = true;
      }

      clearLine();
      getKeyName(currentSelection, keyName);
      char buffer[50];
      sprintf(buffer, "Nuevo mapeo: [%s]", keyName);
      typeText(buffer);

    } else if(currentState == WAITING_NEXT_ACTION) {
      exitConfigMode();
    }
  }

  void confirmMapping() {
    BUTTON_MAP[selectedButton].keycode = currentSelection;
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

  void exitConfigMode() {
    typeText("=== CONFIGURACION GUARDADA ===");
    typeNewline();
    typeNewline();

    currentState = IDLE;
    selectedButton = -1;
  }

  void checkTimeout() {
    if(currentState != IDLE && currentState != CHECKING_ENTRY) {
      if(millis() - stateStartTime >= CONFIG_TIMEOUT) {
        typeNewline();
        typeText("Timeout - Saliendo del modo configuracion");
        exitConfigMode();
      }
    }
  }

  bool isActive() {
    return currentState != IDLE;
  }

  State getState() {
    return currentState;
  }
};

#endif
