#ifndef DEBOUNCE_H
#define DEBOUNCE_H

#include "config.h"

// ============= DEBOUNCE SIMPLE BASE =============
class SimpleDebounce {
protected:
  unsigned long lastTime;
  bool lastState;
  unsigned long debounceDelay;
  
public:
  SimpleDebounce(unsigned long delay = BUTTON_DEBOUNCE_DELAY) 
    : lastTime(0), lastState(false), debounceDelay(delay) {}
  
  bool update(bool currentState) {
    unsigned long currentTime = millis();
    
    if (currentState != lastState) {
      if (currentTime - lastTime >= debounceDelay) {
        lastTime = currentTime;
        lastState = currentState;
        return true; // Estado cambió de forma estable
      }
    }
    return false; // No hay cambio estable
  }
  
  bool getState() { return lastState; }
  
  void setDelay(unsigned long delay) { debounceDelay = delay; }
};

// ============= DEBOUNCE SOFISTICADO BASE =============
class AdvancedDebounce {
protected:
  bool samples[DEBOUNCE_SAMPLES];
  uint8_t index;
  bool stableState;
  unsigned long lastSampleTime;
  unsigned long sampleInterval;
  
public:
  AdvancedDebounce(unsigned long interval = 5) 
    : index(0), stableState(false), lastSampleTime(0), sampleInterval(interval) {
    for(int i = 0; i < DEBOUNCE_SAMPLES; i++) {
      samples[i] = false;
    }
  }
  
  bool update(bool currentState) {
    unsigned long currentTime = millis();
    
    // Solo tomar muestra si pasó el intervalo
    if(currentTime - lastSampleTime < sampleInterval) {
      return false;
    }
    lastSampleTime = currentTime;
    
    samples[index] = currentState;
    index = (index + 1) % DEBOUNCE_SAMPLES;
    
    // Verificar si todas las muestras son iguales
    bool allSame = true;
    bool firstSample = samples[0];
    
    for(int i = 1; i < DEBOUNCE_SAMPLES; i++) {
      if(samples[i] != firstSample) {
        allSame = false;
        break;
      }
    }
    
    if(allSame && firstSample != stableState) {
      stableState = firstSample;
      return true; // Estado cambió de forma estable
    }
    
    return false; // No hay cambio estable
  }
  
  bool getState() { return stableState; }
  
  void setSampleInterval(unsigned long interval) { sampleInterval = interval; }
};

// ============= DEBOUNCE PARA BOTONES =============
class ButtonDebounce {
private:
  SimpleDebounce simple;
  AdvancedDebounce advanced;
  bool useSimple;
  
public:
  ButtonDebounce() : 
    simple(BUTTON_DEBOUNCE_DELAY), 
    advanced(BUTTON_DEBOUNCE_DELAY / DEBOUNCE_SAMPLES),
    useSimple(DEBOUNCE_SIMPLE) {}
  
  bool update(bool currentState) {
    if(useSimple) {
      return simple.update(currentState);
    } else {
      return advanced.update(currentState);
    }
  }
  
  bool getState() {
    if(useSimple) {
      return simple.getState();
    } else {
      return advanced.getState();
    }
  }
  
  void setMode(bool simpleMode) {
    useSimple = simpleMode;
  }
};

// ============= DEBOUNCE ESPECIALIZADO PARA ENCODERS =============
class EncoderDebounce {
private:
  unsigned long lastChangeTime;
  uint8_t lastState;
  uint8_t stableState;
  uint8_t changeCount;
  
public:
  EncoderDebounce() : 
    lastChangeTime(0), 
    lastState(0), 
    stableState(0), 
    changeCount(0) {}
  
  // Actualizar con estado de 2 bits (A y B del encoder)
  bool update(bool pinA, bool pinB) {
    uint8_t currentState = (pinA << 1) | pinB;
    unsigned long currentTime = millis();
    
    if(currentState != lastState) {
      // Cambio detectado
      if(currentTime - lastChangeTime < ENCODER_DEBOUNCE_DELAY) {
        // Cambio muy rápido, posible rebote
        changeCount++;
        
        if(changeCount > 3) {
          // Demasiados cambios rápidos, ignorar
          return false;
        }
      } else {
        // Cambio después de tiempo estable
        changeCount = 0;
      }
      
      lastChangeTime = currentTime;
      lastState = currentState;
      
      // Solo reportar cambio si es diferente al estado estable
      if(currentState != stableState) {
        stableState = currentState;
        return true;
      }
    } else {
      // Sin cambios, resetear contador
      if(currentTime - lastChangeTime > ENCODER_DEBOUNCE_DELAY) {
        changeCount = 0;
      }
    }
    
    return false;
  }
  
  uint8_t getState() { return stableState; }
  
  bool getPinA() { return (stableState >> 1) & 1; }
  bool getPinB() { return stableState & 1; }
};

// ============= DEBOUNCE CON ESTADÍSTICAS =============
class DebouncedButton {
private:
  ButtonDebounce debouncer;
  bool lastStableState;
  
  // Estadísticas
  unsigned long pressCount;
  unsigned long releaseCount;
  unsigned long lastPressTime;
  unsigned long lastReleaseTime;
  unsigned long longestPress;
  unsigned long shortestPress;
  unsigned long bounceCount;
  
public:
  DebouncedButton() : 
    lastStableState(false),
    pressCount(0),
    releaseCount(0),
    lastPressTime(0),
    lastReleaseTime(0),
    longestPress(0),
    shortestPress(0xFFFFFFFF),
    bounceCount(0) {}
  
  bool update(bool currentRawState) {
    bool changed = debouncer.update(currentRawState);
    
    if(changed) {
      bool newState = debouncer.getState();
      
      if(newState && !lastStableState) {
        // Transición a presionado
        pressCount++;
        lastPressTime = millis();
        
      } else if(!newState && lastStableState) {
        // Transición a liberado
        releaseCount++;
        lastReleaseTime = millis();
        
        // Calcular duración de la pulsación
        if(lastPressTime > 0) {
          unsigned long duration = lastReleaseTime - lastPressTime;
          if(duration > longestPress) longestPress = duration;
          if(duration < shortestPress) shortestPress = duration;
        }
      }
      
      lastStableState = newState;
      return true;
    }
    
    // Detectar rebotes (cambios en raw sin cambio en estable)
    static bool lastRawState = false;
    if(currentRawState != lastRawState) {
      bounceCount++;
      lastRawState = currentRawState;
    }
    
    return false;
  }
  
  bool isPressed() { return lastStableState; }
  
  bool wasPressed() {
    return lastStableState && (millis() - lastPressTime < 50);
  }
  
  bool wasReleased() {
    return !lastStableState && (millis() - lastReleaseTime < 50);
  }
  
  // Detección de patrones
  bool isLongPress(unsigned long threshold = 1000) {
    if(!lastStableState) return false;
    return (millis() - lastPressTime) >= threshold;
  }
  
  bool isDoubleClick(unsigned long maxInterval = 500) {
    if(pressCount < 2) return false;
    
    // Verificar si hubo 2 pulsaciones recientes
    static unsigned long lastDoubleCheckTime = 0;
    static uint8_t lastPressCountCheck = 0;
    
    if(pressCount > lastPressCountCheck) {
      if(millis() - lastDoubleCheckTime < maxInterval) {
        lastPressCountCheck = pressCount;
        return true;
      }
      lastDoubleCheckTime = millis();
      lastPressCountCheck = pressCount;
    }
    
    return false;
  }
  
  // Obtener estadísticas
  void getStats(unsigned long* presses, unsigned long* bounces, unsigned long* longest) {
    *presses = pressCount;
    *bounces = bounceCount;
    *longest = longestPress;
  }
  
  void resetStats() {
    pressCount = 0;
    releaseCount = 0;
    bounceCount = 0;
    longestPress = 0;
    shortestPress = 0xFFFFFFFF;
  }
};

// ============= DEBOUNCE GRUPAL =============
class GroupDebounce {
private:
  static const uint8_t MAX_BUTTONS = 16;
  DebouncedButton buttons[MAX_BUTTONS];
  uint16_t lastGroupState;
  uint16_t currentGroupState;
  
public:
  GroupDebounce() : lastGroupState(0), currentGroupState(0) {}
  
  // Actualizar todos los botones de una vez
  bool updateAll(uint16_t rawState) {
    bool anyChanged = false;
    currentGroupState = 0;
    
    for(uint8_t i = 0; i < MAX_BUTTONS; i++) {
      bool pinState = (rawState >> i) & 1;
      
      if(buttons[i].update(pinState)) {
        anyChanged = true;
      }
      
      if(buttons[i].isPressed()) {
        currentGroupState |= (1 << i);
      }
    }
    
    return anyChanged;
  }
  
  // Obtener botón individual
  DebouncedButton* getButton(uint8_t index) {
    if(index >= MAX_BUTTONS) return nullptr;
    return &buttons[index];
  }
  
  // Verificar múltiples botones presionados
  bool arePressed(uint16_t mask) {
    return (currentGroupState & mask) == mask;
  }
  
  // Obtener estado completo
  uint16_t getState() { return currentGroupState; }
  
  // Detectar combos
  bool checkCombo(uint16_t comboMask, unsigned long timeWindow = 100) {
    static unsigned long lastComboTime = 0;
    static uint16_t lastComboState = 0;
    
    if(arePressed(comboMask)) {
      if(lastComboState != comboMask) {
        lastComboState = comboMask;
        lastComboTime = millis();
        return true;
      }
    } else {
      if(millis() - lastComboTime > timeWindow) {
        lastComboState = 0;
      }
    }
    
    return false;
  }
};

#endif