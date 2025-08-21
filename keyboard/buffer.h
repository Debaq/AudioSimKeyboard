#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>

// ============= CONFIGURACIÓN DEL BUFFER =============
#define BUFFER_SIZE 32  // Tamaño del buffer circular

// Estructura para eventos de teclas
struct KeyEvent {
  uint8_t keycode;      // Código de la tecla
  bool isPressed;       // true = presionada, false = liberada
  unsigned long timestamp;  // Cuándo ocurrió
};

// ============= CLASE BUFFER CIRCULAR =============
class CircularBuffer {
private:
  KeyEvent buffer[BUFFER_SIZE];
  volatile uint8_t head;  // Donde se escribe
  volatile uint8_t tail;  // Donde se lee
  volatile uint8_t count; // Elementos en el buffer
  
public:
  CircularBuffer() {
    clear();
  }
  
  // Limpiar buffer
  void clear() {
    head = 0;
    tail = 0;
    count = 0;
  }
  
  // Verificar si está vacío
  bool isEmpty() {
    return count == 0;
  }
  
  // Verificar si está lleno
  bool isFull() {
    return count >= BUFFER_SIZE;
  }
  
  // Cantidad de elementos
  uint8_t getCount() {
    return count;
  }
  
  // Espacio disponible
  uint8_t getSpace() {
    return BUFFER_SIZE - count;
  }
  
  // Agregar evento al buffer
  bool push(uint8_t keycode, bool pressed) {
    // Si está lleno, descartar el más antiguo
    if(isFull()) {
      // Opcional: podrías registrar esto como overflow
      tail = (tail + 1) % BUFFER_SIZE;
      count--;
    }
    
    // Agregar nuevo evento
    buffer[head].keycode = keycode;
    buffer[head].isPressed = pressed;
    buffer[head].timestamp = millis();
    
    head = (head + 1) % BUFFER_SIZE;
    count++;
    
    return true;
  }
  
  // Agregar solo evento de tecla presionada
  bool pushKey(uint8_t keycode) {
    return push(keycode, true);
  }
  
  // Obtener siguiente evento sin removerlo
  bool peek(KeyEvent* event) {
    if(isEmpty()) {
      return false;
    }
    
    *event = buffer[tail];
    return true;
  }
  
  // Obtener y remover siguiente evento
  bool pop(KeyEvent* event) {
    if(isEmpty()) {
      return false;
    }
    
    *event = buffer[tail];
    tail = (tail + 1) % BUFFER_SIZE;
    count--;
    
    return true;
  }
  
  // Procesar todos los eventos pendientes
  uint8_t processAll(void (*callback)(KeyEvent*)) {
    uint8_t processed = 0;
    KeyEvent event;
    
    while(pop(&event)) {
      callback(&event);
      processed++;
    }
    
    return processed;
  }
  
  // Obtener estadísticas para debug
  void getStats(uint8_t* head_pos, uint8_t* tail_pos, uint8_t* elements) {
    *head_pos = head;
    *tail_pos = tail;
    *elements = count;
  }
  
  // Verificar si hay eventos muy antiguos (posible problema)
  bool hasStaleEvents(unsigned long maxAge) {
    if(isEmpty()) {
      return false;
    }
    
    unsigned long now = millis();
    unsigned long eventAge = now - buffer[tail].timestamp;
    
    return eventAge > maxAge;
  }
};

// ============= BUFFER PARA COMBOS/SECUENCIAS =============
class ComboBuffer {
private:
  uint8_t keys[8];      // Máximo 8 teclas en combo
  uint8_t count;
  unsigned long firstKeyTime;
  unsigned long lastKeyTime;
  
public:
  ComboBuffer() {
    clear();
  }
  
  void clear() {
    count = 0;
    firstKeyTime = 0;
    lastKeyTime = 0;
  }
  
  // Agregar tecla al combo
  bool addKey(uint8_t keycode) {
    if(count >= 8) {
      return false;  // Combo muy largo
    }
    
    unsigned long now = millis();
    
    // Si es la primera tecla o pasó mucho tiempo, reiniciar
    if(count == 0 || (now - lastKeyTime) > 500) {
      clear();
      firstKeyTime = now;
    }
    
    keys[count++] = keycode;
    lastKeyTime = now;
    
    return true;
  }
  
  // Verificar si coincide con un patrón
  bool matches(const uint8_t* pattern, uint8_t patternLength) {
    if(count != patternLength) {
      return false;
    }
    
    for(uint8_t i = 0; i < count; i++) {
      if(keys[i] != pattern[i]) {
        return false;
      }
    }
    
    return true;
  }
  
  // Obtener duración del combo
  unsigned long getDuration() {
    if(count == 0) return 0;
    return lastKeyTime - firstKeyTime;
  }
  
  uint8_t getCount() {
    return count;
  }
  
  // Timeout automático
  bool checkTimeout(unsigned long timeout) {
    if(count > 0 && (millis() - lastKeyTime) > timeout) {
      clear();
      return true;
    }
    return false;
  }
};

// ============= SISTEMA DE PRIORIDADES =============
class PriorityKeyBuffer {
private:
  struct PriorityEvent {
    KeyEvent event;
    uint8_t priority;  // 0 = máxima prioridad
  };
  
  PriorityEvent buffer[BUFFER_SIZE];
  uint8_t count;
  
public:
  PriorityKeyBuffer() : count(0) {}
  
  // Agregar con prioridad
  bool push(uint8_t keycode, uint8_t priority = 5) {
    if(count >= BUFFER_SIZE) {
      // Buscar y reemplazar el de menor prioridad
      uint8_t lowestPriority = 0;
      uint8_t lowestIndex = 0;
      
      for(uint8_t i = 0; i < count; i++) {
        if(buffer[i].priority > lowestPriority) {
          lowestPriority = buffer[i].priority;
          lowestIndex = i;
        }
      }
      
      // Solo reemplazar si la nueva tiene mayor prioridad
      if(priority < lowestPriority) {
        buffer[lowestIndex].event.keycode = keycode;
        buffer[lowestIndex].event.isPressed = true;
        buffer[lowestIndex].event.timestamp = millis();
        buffer[lowestIndex].priority = priority;
        return true;
      }
      
      return false;  // Buffer lleno y nueva tecla tiene menor prioridad
    }
    
    // Agregar al final
    buffer[count].event.keycode = keycode;
    buffer[count].event.isPressed = true;
    buffer[count].event.timestamp = millis();
    buffer[count].priority = priority;
    count++;
    
    // Ordenar por prioridad
    sortByPriority();
    
    return true;
  }
  
  // Obtener el de mayor prioridad
  bool pop(KeyEvent* event) {
    if(count == 0) {
      return false;
    }
    
    *event = buffer[0].event;
    
    // Mover todos los demás una posición
    for(uint8_t i = 1; i < count; i++) {
      buffer[i-1] = buffer[i];
    }
    
    count--;
    return true;
  }
  
private:
  void sortByPriority() {
    // Bubble sort simple (suficiente para buffer pequeño)
    for(uint8_t i = 0; i < count - 1; i++) {
      for(uint8_t j = 0; j < count - i - 1; j++) {
        if(buffer[j].priority > buffer[j+1].priority) {
          PriorityEvent temp = buffer[j];
          buffer[j] = buffer[j+1];
          buffer[j+1] = temp;
        }
      }
    }
  }
};

#endif