#ifndef ENCODER_H
#define ENCODER_H

#include "config.h"
#include "debounce.h"

// ============= CLASE ENCODER ROTATIVO MEJORADA =============
class RotaryEncoder {
private:
  uint8_t pinA, pinB;
  
  // Máquina de estados para detección precisa
  enum EncoderState {
    STATE_00 = 0,
    STATE_01 = 1, 
    STATE_10 = 2,
    STATE_11 = 3
  };
  
  EncoderState currentState;
  EncoderState lastValidState;
  
  // Debounce especializado
  EncoderDebounce debouncer;
  
  // Estadísticas y detección de velocidad
  unsigned long lastEventTime;
  unsigned long eventCount;
  int8_t lastDirection;
  uint8_t speed;  // 0=detenido, 1=lento, 2=medio, 3=rápido
  
  // Buffer para suavizar lectura
  int8_t directionBuffer[4];
  uint8_t bufferIndex;
  
  // Detección de errores
  uint8_t errorCount;
  bool isValid;
  
public:
  RotaryEncoder(uint8_t pin_a, uint8_t pin_b) : 
    pinA(pin_a), 
    pinB(pin_b),
    currentState(STATE_00),
    lastValidState(STATE_00),
    lastEventTime(0),
    eventCount(0),
    lastDirection(0),
    speed(0),
    bufferIndex(0),
    errorCount(0),
    isValid(true) {
    
    // Configurar pines con pull-up interno
    pinMode(pinA, INPUT_PULLUP);
    pinMode(pinB, INPUT_PULLUP);
    
    // Leer estado inicial
    bool initA = digitalRead(pinA);
    bool initB = digitalRead(pinB);
    currentState = (EncoderState)((initA << 1) | initB);
    lastValidState = currentState;
    
    // Inicializar buffer
    for(int i = 0; i < 4; i++) {
      directionBuffer[i] = 0;
    }
  }
  
  // Leer dirección con detección de velocidad
  int8_t readDirection() {
    // Leer pines con debounce
    bool pinAState = digitalRead(pinA);
    bool pinBState = digitalRead(pinB);
    
    // Aplicar debounce especializado
    if(!debouncer.update(pinAState, pinBState)) {
      return 0; // Sin cambio estable
    }
    
    // Obtener estado después de debounce
    pinAState = debouncer.getPinA();
    pinBState = debouncer.getPinB();
    
    EncoderState newState = (EncoderState)((pinAState << 1) | pinBState);
    
    // Si no hay cambio, retornar
    if(newState == currentState) {
      updateSpeed(0);
      return 0;
    }
    
    // Detectar dirección basándose en transición
    int8_t direction = getDirectionFromTransition(currentState, newState);
    
    // Validar transición
    if(direction == 0) {
      // Transición inválida
      handleInvalidTransition(currentState, newState);
      currentState = newState;
      return 0;
    }
    
    // Actualizar estados
    lastValidState = currentState;
    currentState = newState;
    
    // Actualizar velocidad
    updateSpeed(direction);
    
    // Aplicar filtro de dirección
    direction = filterDirection(direction);
    
    // Actualizar estadísticas
    lastDirection = direction;
    eventCount++;
    
    return direction;
  }
  
  // Obtener dirección con aceleración (más pasos si gira rápido)
  int8_t readDirectionWithAcceleration() {
    int8_t direction = readDirection();
    
    if(direction == 0) return 0;
    
    // Multiplicar según velocidad
    switch(speed) {
      case 0: return 0;
      case 1: return direction;      // Lento: 1 paso
      case 2: return direction * 2;  // Medio: 2 pasos
      case 3: return direction * 3;  // Rápido: 3 pasos
      default: return direction;
    }
  }
  
  // Obtener velocidad actual
  uint8_t getSpeed() { return speed; }
  
  // Verificar si encoder está funcionando correctamente
  bool isWorking() { return isValid && (errorCount < 10); }
  
  // Resetear estadísticas
  void reset() {
    errorCount = 0;
    eventCount = 0;
    speed = 0;
    isValid = true;
    
    // Re-leer estado actual
    bool pinAState = digitalRead(pinA);
    bool pinBState = digitalRead(pinB);
    currentState = (EncoderState)((pinAState << 1) | pinBState);
    lastValidState = currentState;
  }
  
  // Obtener estadísticas
  void getStats(unsigned long* events, uint8_t* errors, uint8_t* currentSpeed) {
    *events = eventCount;
    *errors = errorCount;
    *currentSpeed = speed;
  }
  
private:
  // Determinar dirección desde transición de estados
  int8_t getDirectionFromTransition(EncoderState from, EncoderState to) {
    // Tabla de transiciones válidas para encoder en cuadratura
    // Retorna: -1=antihorario, 0=inválido, 1=horario
    
    static const int8_t transitionTable[4][4] = {
      // A: STATE_00 -> 
      {  0, -1,  1,  0 },  // 00->00=stop, 00->01=CCW, 00->10=CW, 00->11=invalid
      // B: STATE_01 ->
      {  1,  0,  0, -1 },  // 01->00=CW, 01->01=stop, 01->10=invalid, 01->11=CCW
      // C: STATE_10 ->
      { -1,  0,  0,  1 },  // 10->00=CCW, 10->01=invalid, 10->10=stop, 10->11=CW
      // D: STATE_11 ->
      {  0,  1, -1,  0 }   // 11->00=invalid, 11->01=CW, 11->10=CCW, 11->11=stop
    };
    
    return transitionTable[from][to];
  }
  
  // Manejar transición inválida
  void handleInvalidTransition(EncoderState from, EncoderState to) {
    errorCount++;
    
    if(errorCount > 5) {
      // Muchos errores, marcar como problemático
      isValid = false;
      
      if(errorCount == 6) {
        Serial.print("Encoder ");
        Serial.print((pinA == ENCODER_A_PIN1) ? 'A' : 'B');
        Serial.println(" detectando transiciones invalidas");
      }
    }
    
    // Log para debug si está habilitado
    #if DEBUG_MODE
    if(errorCount <= 5) {
      Serial.print("Transicion invalida: ");
      Serial.print(from);
      Serial.print(" -> ");
      Serial.println(to);
    }
    #endif
  }
  
  // Actualizar detección de velocidad
  void updateSpeed(int8_t direction) {
    unsigned long now = millis();
    unsigned long timeDelta = now - lastEventTime;
    
    if(direction == 0) {
      // Sin movimiento
      if(timeDelta > 500) {
        speed = 0;
      }
      return;
    }
    
    lastEventTime = now;
    
    // Calcular velocidad basada en tiempo entre eventos
    if(timeDelta < 10) {
      speed = 3;  // Muy rápido
    } else if(timeDelta < 50) {
      speed = 2;  // Medio
    } else if(timeDelta < 200) {
      speed = 1;  // Lento
    } else {
      speed = 0;  // Detenido/iniciando
    }
  }
  
  // Filtrar dirección para evitar ruido
  int8_t filterDirection(int8_t newDirection) {
    // Agregar al buffer circular
    directionBuffer[bufferIndex] = newDirection;
    bufferIndex = (bufferIndex + 1) % 4;
    
    // Contar direcciones en el buffer
    int8_t sum = 0;
    for(int i = 0; i < 4; i++) {
      sum += directionBuffer[i];
    }
    
    // Si mayoría va en una dirección, confirmarla
    if(sum >= 2) return 1;   // Mayoría horario
    if(sum <= -2) return -1;  // Mayoría antihorario
    
    // Si no hay consenso, usar la última válida
    return newDirection;
  }
};

// ============= GESTOR DE MÚLTIPLES ENCODERS =============
class EncoderManager {
private:
  static const uint8_t MAX_ENCODERS = 2;
  RotaryEncoder* encoders[MAX_ENCODERS];
  uint8_t encoderCount;
  
  // Para detección de gestos
  unsigned long gestureStartTime;
  int8_t gestureBuffer[2][10];  // Últimos 10 movimientos por encoder
  uint8_t gestureIndex[2];
  
public:
  EncoderManager() : encoderCount(0), gestureStartTime(0) {
    for(int i = 0; i < MAX_ENCODERS; i++) {
      encoders[i] = nullptr;
      gestureIndex[i] = 0;
      
      for(int j = 0; j < 10; j++) {
        gestureBuffer[i][j] = 0;
      }
    }
  }
  
  // Agregar encoder
  bool addEncoder(RotaryEncoder* encoder) {
    if(encoderCount >= MAX_ENCODERS) return false;
    
    encoders[encoderCount++] = encoder;
    return true;
  }
  
  // Procesar todos los encoders
  void updateAll() {
    for(uint8_t i = 0; i < encoderCount; i++) {
      if(encoders[i] != nullptr) {
        int8_t dir = encoders[i]->readDirection();
        
        if(dir != 0) {
          // Agregar a buffer de gestos
          recordGesture(i, dir);
        }
      }
    }
  }
  
  // Obtener encoder específico
  RotaryEncoder* getEncoder(uint8_t index) {
    if(index >= encoderCount) return nullptr;
    return encoders[index];
  }
  
  // Detectar gesto de giro simultáneo
  bool detectSimultaneousTurn(int8_t* dirA, int8_t* dirB, unsigned long window = 100) {
    if(encoderCount < 2) return false;
    
    static unsigned long lastEventA = 0;
    static unsigned long lastEventB = 0;
    static int8_t lastDirA = 0;
    static int8_t lastDirB = 0;
    
    int8_t currentDirA = encoders[0]->readDirection();
    int8_t currentDirB = encoders[1]->readDirection();
    
    unsigned long now = millis();
    
    if(currentDirA != 0) {
      lastEventA = now;
      lastDirA = currentDirA;
    }
    
    if(currentDirB != 0) {
      lastEventB = now;
      lastDirB = currentDirB;
    }
    
    // Verificar si ambos eventos ocurrieron dentro de la ventana
    if(abs((long)lastEventA - (long)lastEventB) < window) {
      if(lastDirA != 0 && lastDirB != 0) {
        *dirA = lastDirA;
        *dirB = lastDirB;
        
        // Limpiar para no detectar múltiples veces
        lastDirA = 0;
        lastDirB = 0;
        
        return true;
      }
    }
    
    return false;
  }
  
  // Detectar patrón de giro (ej: derecha-derecha-izquierda)
  bool detectPattern(uint8_t encoderIndex, const int8_t* pattern, uint8_t patternLength) {
    if(encoderIndex >= encoderCount) return false;
    
    // Verificar si hay suficientes movimientos
    uint8_t startIdx = (gestureIndex[encoderIndex] + 10 - patternLength) % 10;
    
    for(uint8_t i = 0; i < patternLength; i++) {
      uint8_t bufIdx = (startIdx + i) % 10;
      if(gestureBuffer[encoderIndex][bufIdx] != pattern[i]) {
        return false;
      }
    }
    
    return true;
  }
  
  // Estado de salud de todos los encoders
  bool allEncodersHealthy() {
    for(uint8_t i = 0; i < encoderCount; i++) {
      if(encoders[i] != nullptr && !encoders[i]->isWorking()) {
        return false;
      }
    }
    return true;
  }
  
  // Debug info
  void printStats() {
    Serial.println("=== ENCODER STATS ===");
    
    for(uint8_t i = 0; i < encoderCount; i++) {
      if(encoders[i] != nullptr) {
        unsigned long events;
        uint8_t errors, speed;
        
        encoders[i]->getStats(&events, &errors, &speed);
        
        Serial.print("Encoder ");
        Serial.print(i);
        Serial.print(": Events=");
        Serial.print(events);
        Serial.print(" Errors=");
        Serial.print(errors);
        Serial.print(" Speed=");
        Serial.print(speed);
        Serial.print(" Health=");
        Serial.println(encoders[i]->isWorking() ? "OK" : "ERROR");
      }
    }
  }
  
private:
  void recordGesture(uint8_t encoderIndex, int8_t direction) {
    gestureBuffer[encoderIndex][gestureIndex[encoderIndex]] = direction;
    gestureIndex[encoderIndex] = (gestureIndex[encoderIndex] + 1) % 10;
  }
};

#endif