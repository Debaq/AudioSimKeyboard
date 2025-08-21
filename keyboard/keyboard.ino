#include <Wire.h>
#include <PCF8575.h>
#include <USBComposite.h>
#include "config.h"
#include "debounce.h"
#include "encoder.h"
#include "buffer.h"
#include "watchdog.h"
#include "storage.h"
#include "config_mode.h"

// ============= OBJETOS GLOBALES =============
PCF8575 pcf8575(PCF8575_ADDRESS);
USBHID HID;
HIDKeyboard Keyboard(HID);

// Sistema de debounce mejorado
GroupDebounce buttonDebouncer;
bool lastButtonState[16];

// Encoders con detección mejorada
RotaryEncoder encoderA(ENCODER_A_PIN1, ENCODER_A_PIN2);
RotaryEncoder encoderB(ENCODER_B_PIN1, ENCODER_B_PIN2);
EncoderManager encoderManager;

// Buffer circular para teclas
CircularBuffer keyBuffer;
ComboBuffer comboBuffer;

// Watchdog y monitoreo de salud
WatchdogManager watchdog;
SystemHealthMonitor* healthMonitor;
RecoveryManager recovery;

// Modo configuración
ConfigMode* configMode;

// Timing no bloqueante
unsigned long lastMainLoop = 0;
unsigned long lastI2CCheck = 0;
unsigned long lastDebugPrint = 0;
unsigned long loopStartTime = 0;

// Estadísticas del sistema
SystemStats systemStats = {0, 0, 0, 0, 0, 0, 0};

// Estado de conexión
bool pcf8575Connected = false;
uint8_t pcf8575RetryCount = 0;

// ============= FUNCIÓN SETUP =============
void setup() {
  // Inicializar Serial
  Serial.begin(SERIAL_BAUD);
  Serial.println("=== INICIANDO TECLADO USB AVANZADO ===");
  
  // Inicializar almacenamiento y cargar configuración
  Serial.println("Cargando configuracion...");
  initStorage();
  
  // Inicializar Watchdog
  Serial.println("Configurando watchdog...");
  if(watchdog.init()) {
    healthMonitor = new SystemHealthMonitor(&watchdog);
  }
  
  // Inicializar I2C
  Serial.println("Iniciando I2C...");
  Wire.begin();
  Wire.setClock(400000); // 400kHz para mejor velocidad
  
  // Intentar conectar con PCF8575
  connectPCF8575();
  
  // Configurar encoders
  Serial.println("Configurando encoders...");
  encoderManager.addEncoder(&encoderA);
  encoderManager.addEncoder(&encoderB);
  
  // Inicializar USB HID
  Serial.println("Iniciando USB HID...");
  HID.begin(HID_KEYBOARD);
  Keyboard.begin();
  
  // Inicializar modo configuración
  configMode = new ConfigMode(&Keyboard);
  
  // Esperar conexión USB
  unsigned long usbWaitStart = millis();
  while(!USBComposite) {
    if(millis() - usbWaitStart > 3000) {
      Serial.println("Timeout esperando USB");
      break;
    }
    delay(100);
  }
  
  // Registrar tiempo de inicio
  systemStats.startTime = millis();
  
  // Mostrar configuración actual
  printCurrentConfiguration();
  
  Serial.println("=== SISTEMA LISTO ===");
  
  // Si hubo reset por watchdog, notificar
  if(watchdog.wasResetByWatchdog()) {
    delay(1000);
    Keyboard.print("[Sistema recuperado de error]");
  }
}

// ============= LOOP PRINCIPAL NO BLOQUEANTE =============
void loop() {
  loopStartTime = millis();
  
  // Verificar modo recuperación
  if(recovery.isInRecovery()) {
    if(recovery.attemptRecovery()) {
      recovery.exitRecoveryMode();
    }
    return;
  }
  
  // Timing no bloqueante para loop principal
  if(millis() - lastMainLoop >= MAIN_LOOP_INTERVAL) {
    lastMainLoop = millis();
    
    // Procesar buffer de teclas pendientes
    processKeyBuffer();
    
    // Si no estamos en modo config, procesar entrada normal
    if(!configMode->isActive()) {
      // Procesar botones
      if(pcf8575Connected) {
        processButtons();
      }
      
      // Procesar encoders
      processEncoders();
      
    } else {
      // En modo config, solo verificar timeout
      configMode->checkTimeout();
    }
  }
  
  // Verificación periódica de I2C
  if(millis() - lastI2CCheck >= I2C_CHECK_INTERVAL) {
    lastI2CCheck = millis();
    checkI2CConnection();
  }
  
  // Debug periódico
  #if DEBUG_MODE
  if(millis() - lastDebugPrint >= DEBUG_PRINT_INTERVAL) {
    lastDebugPrint = millis();
    printDebugInfo();
  }
  #endif
  
  // Actualizar métricas de salud
  unsigned long loopTime = millis() - loopStartTime;
  if(loopTime > systemStats.longestLoopTime) {
    systemStats.longestLoopTime = loopTime;
  }
  
  // Verificar salud del sistema y resetear watchdog
  if(healthMonitor) {
    healthMonitor->updateLoopTime(loopTime);
    healthMonitor->performHealthCheck();
  }
  
  // Reset condicional del watchdog
  watchdog.conditionalReset(50);
}

// ============= PROCESAMIENTO DE BOTONES MEJORADO =============
void processButtons() {
  // Leer todos los pines del PCF8575 de una vez
  uint16_t allPins = pcf8575.digitalReadAll();
  
  // Verificar error de lectura
  if(allPins == 0xFFFF && pcf8575RetryCount > 0) {
    // Posible desconexión
    handleI2CError();
    return;
  }
  
  // Aplicar debounce grupal
  if(buttonDebouncer.updateAll(~allPins)) { // Invertir porque botones a GND
    
    // Procesar cada botón
    for(int i = 0; i < 16; i++) {
      DebouncedButton* btn = buttonDebouncer.getButton(i);
      
      if(btn->wasPressed()) {
        handleButtonPress(i);
      } else if(btn->wasReleased()) {
        handleButtonRelease(i);
      }
      
      // Detectar patrones especiales
      if(btn->isLongPress(CONFIG_HOLD_TIME)) {
        // Verificar si es parte del combo de config
        checkConfigEntry();
      }
    }
  }
}

// ============= MANEJO DE PRESIÓN DE BOTÓN =============
void handleButtonPress(uint8_t buttonIndex) {
  // Si estamos en modo config, enviar al manejador
  if(configMode->isActive()) {
    configMode->processButton(buttonIndex);
    return;
  }
  
  // Verificar entrada a modo config
  bool buttonsForConfig[16];
  for(int i = 0; i < 16; i++) {
    buttonsForConfig[i] = buttonDebouncer.getButton(i)->isPressed();
  }
  
  if(configMode->checkEntry(buttonsForConfig)) {
    systemStats.configModeEntries++;
    return; // Entró a modo config
  }
  
  // Operación normal - agregar al buffer
  uint8_t keycode = BUTTON_MAP[buttonIndex].keycode;
  uint8_t priority = getKeyPriority(keycode);
  
  if(!keyBuffer.pushKey(keycode)) {
    systemStats.bufferOverflows++;
    if(healthMonitor) {
      healthMonitor->recordBufferOverflow();
    }
  }
  
  systemStats.keyPresses++;
  
  #if DEBUG_MODE
  Serial.print("Boton ");
  Serial.print(buttonIndex);
  Serial.print(" [");
  Serial.print(BUTTON_MAP[buttonIndex].description);
  Serial.println("] presionado");
  #endif
}

// ============= MANEJO DE LIBERACIÓN DE BOTÓN =============
void handleButtonRelease(uint8_t buttonIndex) {
  // En modo config no hacemos nada al soltar
  if(configMode->isActive()) {
    return;
  }
  
  // Podríamos trackear duración de presión aquí si fuera necesario
}

// ============= PROCESAMIENTO DE ENCODERS MEJORADO =============
void processEncoders() {
  // Encoder A
  int8_t dirA = encoderA.readDirectionWithAcceleration();
  if(dirA != 0) {
    if(configMode->isActive()) {
      configMode->processEncoder(0, dirA);
    } else {
      char key = (dirA > 0) ? ENCODER_MAP[0].right_key : ENCODER_MAP[0].left_key;
      
      for(int i = 0; i < abs(dirA); i++) {
        keyBuffer.pushKey(key);
      }
      
      systemStats.encoderEvents++;
      
      #if DEBUG_MODE
      Serial.print("Encoder A: ");
      Serial.print(dirA > 0 ? "der" : "izq");
      Serial.print(" x");
      Serial.println(abs(dirA));
      #endif
    }
  }
  
  // Encoder B
  int8_t dirB = encoderB.readDirectionWithAcceleration();
  if(dirB != 0) {
    if(configMode->isActive()) {
      configMode->processEncoder(1, dirB);
    } else {
      char key = (dirB > 0) ? ENCODER_MAP[1].right_key : ENCODER_MAP[1].left_key;
      
      for(int i = 0; i < abs(dirB); i++) {
        keyBuffer.pushKey(key);
      }
      
      systemStats.encoderEvents++;
      
      #if DEBUG_MODE
      Serial.print("Encoder B: ");
      Serial.print(dirB > 0 ? "der" : "izq");
      Serial.print(" x");
      Serial.println(abs(dirB));
      #endif
    }
  }
  
  // Detectar gestos especiales con encoders
  int8_t simultDirA, simultDirB;
  if(encoderManager.detectSimultaneousTurn(&simultDirA, &simultDirB)) {
    handleEncoderGesture(simultDirA, simultDirB);
  }
}

// ============= PROCESAR BUFFER DE TECLAS =============
void processKeyBuffer() {
  KeyEvent event;
  uint8_t processed = 0;
  
  // Procesar hasta 5 teclas por ciclo para no bloquear
  while(keyBuffer.pop(&event) && processed < 5) {
    sendKey(event.keycode);
    processed++;
    
    // Pequeña pausa entre teclas
    delay(KEY_RELEASE_DELAY);
  }
  
  // Verificar si el buffer está muy lleno
  if(keyBuffer.getCount() > BUFFER_OVERFLOW_THRESHOLD) {
    Serial.println("¡Buffer cerca del limite!");
  }
}

// ============= ENVÍO DE TECLAS HID =============
void sendKey(uint8_t keycode) {
  // Presionar tecla
  Keyboard.press(keycode);
  delay(KEY_PRESS_DURATION);
  
  // Soltar tecla
  Keyboard.release(keycode);
  
  // Asegurar que se envíe el reporte
  Keyboard.releaseAll();
}

// ============= VERIFICAR ENTRADA A MODO CONFIG =============
void checkConfigEntry() {
  bool buttonsState[16];
  for(int i = 0; i < 16; i++) {
    buttonsState[i] = buttonDebouncer.getButton(i)->isPressed();
  }
  
  if(configMode->checkEntry(buttonsState)) {
    Serial.println("Entrando a modo configuracion...");
    systemStats.configModeEntries++;
  }
}

// ============= MANEJAR GESTOS DE ENCODERS =============
void handleEncoderGesture(int8_t dirA, int8_t dirB) {
  // Ambos encoders hacia la derecha = Page Down
  if(dirA > 0 && dirB > 0) {
    Keyboard.press(KEY_PAGE_DOWN);
    delay(10);
    Keyboard.release(KEY_PAGE_DOWN);
  }
  // Ambos hacia la izquierda = Page Up
  else if(dirA < 0 && dirB < 0) {
    Keyboard.press(KEY_PAGE_UP);
    delay(10);
    Keyboard.release(KEY_PAGE_UP);
  }
  // Direcciones opuestas = podría ser otro comando especial
}

// ============= CONEXIÓN PCF8575 =============
void connectPCF8575() {
  Serial.print("Conectando con PCF8575... ");
  
  for(int retry = 0; retry < PCF8575_MAX_RETRIES; retry++) {
    if(pcf8575.begin()) {
      pcf8575Connected = true;
      pcf8575RetryCount = 0;
      
      // Configurar todos los pines como INPUT con pull-up
      for(int i = 0; i < 16; i++) {
        pcf8575.pinMode(i, INPUT_PULLUP);
        lastButtonState[i] = true;
      }
      
      Serial.println("OK");
      return;
    }
    
    delay(PCF8575_RETRY_DELAY);
  }
  
  Serial.println("FALLO");
  pcf8575Connected = false;
  handleI2CError();
}

// ============= VERIFICACIÓN PERIÓDICA I2C =============
void checkI2CConnection() {
  if(!pcf8575Connected) {
    // Intentar reconectar
    connectPCF8575();
    return;
  }
  
  // Verificar que sigue conectado
  Wire.beginTransmission(PCF8575_ADDRESS);
  uint8_t error = Wire.endTransmission();
  
  if(error != 0) {
    pcf8575Connected = false;
    handleI2CError();
  }
}

// ============= MANEJO DE ERRORES I2C =============
void handleI2CError() {
  systemStats.i2cErrors++;
  
  if(healthMonitor) {
    healthMonitor->recordI2CError();
  }
  
  Serial.print("Error I2C #");
  Serial.println(systemStats.i2cErrors);
  
  // Si hay muchos errores, entrar en modo recuperación
  if(systemStats.i2cErrors > 5 && !recovery.isInRecovery()) {
    recovery.enterRecoveryMode();
  }
}

// ============= FUNCIONES DE DEBUG =============
void printDebugInfo() {
  Serial.println("\n=== INFO DEBUG ===");
  
  // Estado del sistema
  unsigned long uptime = millis() - systemStats.startTime;
  Serial.print("Uptime: ");
  Serial.print(uptime / 1000);
  Serial.println(" segundos");
  
  Serial.print("Teclas: ");
  Serial.print(systemStats.keyPresses);
  Serial.print(" | Encoder: ");
  Serial.print(systemStats.encoderEvents);
  Serial.print(" | Config: ");
  Serial.println(systemStats.configModeEntries);
  
  Serial.print("Errores I2C: ");
  Serial.print(systemStats.i2cErrors);
  Serial.print(" | Buffer overflow: ");
  Serial.print(systemStats.bufferOverflows);
  Serial.print(" | Loop max: ");
  Serial.print(systemStats.longestLoopTime);
  Serial.println("ms");
  
  // Estado de hardware
  Serial.print("PCF8575: ");
  Serial.print(pcf8575Connected ? "OK" : "ERROR");
  Serial.print(" | Encoders: ");
  Serial.println(encoderManager.allEncodersHealthy() ? "OK" : "ERROR");
  
  // Buffer status
  Serial.print("Buffer: ");
  Serial.print(keyBuffer.getCount());
  Serial.print("/");
  Serial.println(BUFFER_SIZE);
  
  // Métricas de salud
  if(healthMonitor) {
    healthMonitor->printMetrics();
  }
  
  // Estado de encoders
  encoderManager.printStats();
  
  Serial.println("==================\n");
}

// ============= COMANDOS SERIAL PARA DEBUG =============
void serialEvent() {
  while(Serial.available()) {
    char cmd = Serial.read();
    
    switch(cmd) {
      case 'd': // Debug info
        printDebugInfo();
        break;
        
      case 'r': // Reset estadísticas
        systemStats.keyPresses = 0;
        systemStats.encoderEvents = 0;
        systemStats.i2cErrors = 0;
        systemStats.bufferOverflows = 0;
        systemStats.longestLoopTime = 0;
        Serial.println("Estadisticas reseteadas");
        break;
        
      case 'c': // Mostrar configuración
        printCurrentConfiguration();
        break;
        
      case 'R': // Reset a defaults
        resetToDefaults();
        break;
        
      case 's': // Guardar config actual
        saveConfiguration();
        break;
        
      case 'h': // Ayuda
        Serial.println("Comandos:");
        Serial.println("d - Debug info");
        Serial.println("r - Reset stats");
        Serial.println("c - Mostrar config");
        Serial.println("R - Reset a defaults");
        Serial.println("s - Guardar config");
        Serial.println("h - Esta ayuda");
        break;
    }
  }
}