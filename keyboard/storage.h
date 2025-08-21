#ifndef STORAGE_H
#define STORAGE_H

#include <EEPROM.h>
#include "config.h"

// ============= CONFIGURACIÓN DE ALMACENAMIENTO =============
#define STORAGE_VERSION 0x01        // Versión del formato de almacenamiento
#define STORAGE_MAGIC 0xBEEF        // Número mágico para validación
#define STORAGE_START_ADDR 0        // Dirección inicial en EEPROM

// Estructura para almacenar la configuración
struct StorageData {
  uint16_t magic;                   // Validación de datos
  uint8_t version;                  // Versión del formato
  uint8_t keycodes[16];             // Mapeo de las 16 teclas
  uint8_t encoderAKeys[2];          // Izq/Der encoder A
  uint8_t encoderBKeys[2];          // Izq/Der encoder B
  uint8_t checksum;                 // Checksum simple
};

// ============= FUNCIONES DE ALMACENAMIENTO =============

// Calcular checksum
uint8_t calculateChecksum(StorageData* data) {
  uint8_t sum = 0;
  uint8_t* ptr = (uint8_t*)data;
  
  // Sumar todos los bytes excepto el checksum mismo
  for(size_t i = 0; i < sizeof(StorageData) - 1; i++) {
    sum += ptr[i];
  }
  
  return ~sum;  // Complemento a 1
}

// Validar datos almacenados
bool validateStorage(StorageData* data) {
  // Verificar magic number
  if(data->magic != STORAGE_MAGIC) {
    return false;
  }
  
  // Verificar versión
  if(data->version != STORAGE_VERSION) {
    return false;
  }
  
  // Verificar checksum
  uint8_t calculated = calculateChecksum(data);
  if(calculated != data->checksum) {
    return false;
  }
  
  return true;
}

// Guardar configuración actual en EEPROM
void saveConfiguration() {
  StorageData data;
  
  // Preparar estructura
  data.magic = STORAGE_MAGIC;
  data.version = STORAGE_VERSION;
  
  // Copiar mapeo de botones
  for(int i = 0; i < 16; i++) {
    data.keycodes[i] = BUTTON_MAP[i].keycode;
  }
  
  // Copiar mapeo de encoders
  data.encoderAKeys[0] = ENCODER_MAP[0].left_key;
  data.encoderAKeys[1] = ENCODER_MAP[0].right_key;
  data.encoderBKeys[0] = ENCODER_MAP[1].left_key;
  data.encoderBKeys[1] = ENCODER_MAP[1].right_key;
  
  // Calcular y asignar checksum
  data.checksum = calculateChecksum(&data);
  
  // Escribir en EEPROM
  EEPROM.put(STORAGE_START_ADDR, data);
  
  Serial.println("Configuracion guardada en EEPROM");
}

// Cargar configuración desde EEPROM
bool loadConfiguration() {
  StorageData data;
  
  // Leer de EEPROM
  EEPROM.get(STORAGE_START_ADDR, data);
  
  // Validar datos
  if(!validateStorage(&data)) {
    Serial.println("EEPROM sin datos validos o corrupta");
    return false;
  }
  
  // Aplicar configuración a las estructuras en memoria
  for(int i = 0; i < 16; i++) {
    // Actualizar solo el keycode, mantener port/pin/description
    const_cast<KeyMap*>(BUTTON_MAP)[i].keycode = data.keycodes[i];
  }
  
  // Actualizar encoders
  const_cast<EncoderMap*>(ENCODER_MAP)[0].left_key = data.encoderAKeys[0];
  const_cast<EncoderMap*>(ENCODER_MAP)[0].right_key = data.encoderAKeys[1];
  const_cast<EncoderMap*>(ENCODER_MAP)[1].left_key = data.encoderBKeys[0];
  const_cast<EncoderMap*>(ENCODER_MAP)[1].right_key = data.encoderBKeys[1];
  
  Serial.println("Configuracion cargada desde EEPROM");
  return true;
}

// Resetear a configuración por defecto
void resetToDefaults() {
  // Restaurar valores por defecto de config.h
  const uint8_t DEFAULT_KEYCODES[16] = {
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8,
    KEY_F9, KEY_F10, KEY_F11, KEY_F12, 'a', 'b', 'c', 'd'
  };
  
  for(int i = 0; i < 16; i++) {
    const_cast<KeyMap*>(BUTTON_MAP)[i].keycode = DEFAULT_KEYCODES[i];
  }
  
  // Restaurar encoders
  const_cast<EncoderMap*>(ENCODER_MAP)[0].left_key = 'c';
  const_cast<EncoderMap*>(ENCODER_MAP)[0].right_key = 'v';
  const_cast<EncoderMap*>(ENCODER_MAP)[1].left_key = 'b';
  const_cast<EncoderMap*>(ENCODER_MAP)[1].right_key = 'n';
  
  // Guardar defaults en EEPROM
  saveConfiguration();
  
  Serial.println("Configuracion restaurada a valores por defecto");
}

// Inicializar sistema de almacenamiento
void initStorage() {
  // STM32 Blue Pill no tiene EEPROM real, usa Flash emulada
  // La librería EEPROM para STM32 maneja esto automáticamente
  
  Serial.println("Inicializando sistema de almacenamiento...");
  
  // Intentar cargar configuración
  if(!loadConfiguration()) {
    Serial.println("Usando configuracion por defecto");
    // Si falla, guardar la configuración por defecto
    saveConfiguration();
  }
}

// Debug: mostrar configuración actual
void printCurrentConfiguration() {
  Serial.println("=== CONFIGURACION ACTUAL ===");
  
  for(int i = 0; i < 16; i++) {
    Serial.print("Boton ");
    Serial.print(i + 1);
    Serial.print(": ");
    
    uint8_t keycode = BUTTON_MAP[i].keycode;
    if(keycode >= 'a' && keycode <= 'z') {
      Serial.print("'");
      Serial.print((char)keycode);
      Serial.println("'");
    } else if(keycode >= KEY_F1 && keycode <= KEY_F12) {
      Serial.print("F");
      Serial.println((keycode - KEY_F1) + 1);
    } else {
      Serial.print("0x");
      Serial.println(keycode, HEX);
    }
  }
  
  Serial.print("Encoder A: izq='");
  Serial.print((char)ENCODER_MAP[0].left_key);
  Serial.print("' der='");
  Serial.print((char)ENCODER_MAP[0].right_key);
  Serial.println("'");
  
  Serial.print("Encoder B: izq='");
  Serial.print((char)ENCODER_MAP[1].left_key);
  Serial.print("' der='");
  Serial.print((char)ENCODER_MAP[1].right_key);
  Serial.println("'");
  
  Serial.println("========================");
}

#endif