#include <Wire.h>
#include <PCF8575.h>
#include <Keyboard.h>  // En lugar de USBComposite
#include "config.h"
#include "debounce.h"
#include "encoder.h"
#include "buffer.h"
#include "watchdog.h"
#include "storage.h"
#include "config_mode.h"

// ============= OBJETOS GLOBALES =============
PCF8575 pcf8575(PCF8575_ADDRESS);

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
  Wire.setClock(400000);

  // Intentar conectar con PCF8575
  connectPCF8575();

  // Configurar encoders
  Serial.println("Configurando encoders...");
  encoderManager.addEncoder(&encoderA);
  encoderManager.addEncoder(&encoderB);

  // Inicializar USB HID Keyboard
  Serial.println("Iniciando USB HID...");
  Keyboard.begin();

  // Inicializar modo configuración
  configMode = new ConfigMode();

  // Esperar un poco para la conexión USB
  delay(2000);

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
  uint16_t allPins = pcf8575.digitalReadAll();

  if(allPins == 0xFFFF && pcf8575RetryCount > 0) {
    handleI2CError();
    return;
  }

  if(buttonDebouncer.updateAll(~allPins)) {

    for(int i = 0; i < 16; i++) {
      DebouncedButton* btn = buttonDebouncer.getButton(i);

      if(btn->wasPressed()) {
        handleButtonPress(i);
      } else if(btn->wasReleased()) {
        handleButtonRelease(i);
      }

      if(btn->isLongPress(CONFIG_HOLD_TIME)) {
        checkConfigEntry();
      }
    }
  }
}

// ============= MANEJO DE PRESIÓN DE BOTÓN =============
void handleButtonPress(uint8_t buttonIndex) {
  if(configMode->isActive()) {
    configMode->processButton(buttonIndex);
    return;
  }

  bool buttonsForConfig[16];
  for(int i = 0; i < 16; i++) {
    buttonsForConfig[i] = buttonDebouncer.getButton(i)->isPressed();
  }

  if(configMode->checkEntry(buttonsForConfig)) {
    systemStats.configModeEntries++;
    return;
  }

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

void handleButtonRelease(uint8_t buttonIndex) {
  if(configMode->isActive()) {
    return;
  }
}

// ============= PROCESAMIENTO DE ENCODERS MEJORADO =============
void processEncoders() {
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

  int8_t simultDirA, simultDirB;
  if(encoderManager.detectSimultaneousTurn(&simultDirA, &simultDirB)) {
    handleEncoderGesture(simultDirA, simultDirB);
  }
}

// ============= PROCESAR BUFFER DE TECLAS =============
void processKeyBuffer() {
  KeyEvent event;
  uint8_t processed = 0;

  while(keyBuffer.pop(&event) && processed < 5) {
    sendKey(event.keycode);
    processed++;
    delay(KEY_RELEASE_DELAY);
  }

  if(keyBuffer.getCount() > BUFFER_OVERFLOW_THRESHOLD) {
    Serial.println("¡Buffer cerca del limite!");
  }
}

// ============= ENVÍO DE TECLAS HID =============
void sendKey(uint8_t keycode) {
  Keyboard.press(keycode);
  delay(KEY_PRESS_DURATION);
  Keyboard.release(keycode);
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
  if(dirA > 0 && dirB > 0) {
    Keyboard.press(KEY_PAGE_DOWN);
    delay(10);
    Keyboard.release(KEY_PAGE_DOWN);
  }
  else if(dirA < 0 && dirB < 0) {
    Keyboard.press(KEY_PAGE_UP);
    delay(10);
    Keyboard.release(KEY_PAGE_UP);
  }
}

// ============= CONEXIÓN PCF8575 =============
void connectPCF8575() {
  Serial.print("Conectando con PCF8575... ");

  for(int retry = 0; retry < PCF8575_MAX_RETRIES; retry++) {
    if(pcf8575.begin()) {
      pcf8575Connected = true;
      pcf8575RetryCount = 0;

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
#ifndef WATCHDOG_H
#define WATCHDOG_H

// Para STM32F103 sin librería IWatchdog, usamos el IWDG nativo
#include <Arduino.h>

// ============= CONFIGURACIÓN WATCHDOG =============
#define WATCHDOG_TIMEOUT 5000     
#define WATCHDOG_ENABLED true     

// Registros IWDG para STM32F103
#define IWDG_KR   (*(volatile uint32_t*)0x40003000)
#define IWDG_PR   (*(volatile uint32_t*)0x40003004)
#define IWDG_RLR  (*(volatile uint32_t*)0x40003008)
#define IWDG_SR   (*(volatile uint32_t*)0x4000300C)

#define IWDG_START    0xCCCC
#define IWDG_RELOAD   0xAAAA
#define IWDG_UNLOCK   0x5555
#define IWDG_LOCK     0x0000

// ============= CLASE WATCHDOG MANAGER =============
class WatchdogManager {
private:
  bool enabled;
  unsigned long lastReset;
  unsigned long resetCount;
  bool wasWatchdogReset;
  
public:
  WatchdogManager() {
    enabled = WATCHDOG_ENABLED;
    lastReset = 0;
    resetCount = 0;
    wasWatchdogReset = false;
  }
  
  bool init() {
    if(!enabled) {
      Serial.println("Watchdog deshabilitado");
      return false;
    }
    
    // Para STM32F103, configurar IWDG manualmente
    // Prescaler: 64, Reload: 625 para ~5 segundos @ 40kHz LSI
    
    IWDG_KR = IWDG_UNLOCK;  // Desbloquear registros
    IWDG_PR = 0x5;          // Prescaler /64
    IWDG_RLR = 3125;        // 5 segundos aprox
    IWDG_KR = IWDG_START;   // Iniciar watchdog
    IWDG_KR = IWDG_RELOAD;  // Primer reset
    
    Serial.println("Watchdog inicializado: 5 segundos");
    lastReset = millis();
    
    return true;
  }
  
  void reset() {
    if(!enabled) return;
    
    IWDG_KR = IWDG_RELOAD;
    lastReset = millis();
    resetCount++;
  }
  
  void conditionalReset(unsigned long minInterval = 100) {
    if(!enabled) return;
    
    unsigned long now = millis();
    if(now - lastReset >= minInterval) {
      reset();
    }
  }
  
  unsigned long getTimeSinceReset() {
    return millis() - lastReset;
  }
  
  void getStats(unsigned long* resets, unsigned long* lastResetTime, bool* wasReset) {
    *resets = resetCount;
    *lastResetTime = lastReset;
    *wasReset = wasWatchdogReset;
  }
  
  bool isNearTimeout(unsigned long threshold = 1000) {
    if(!enabled) return false;
    
    unsigned long timeSinceReset = getTimeSinceReset();
    return timeSinceReset >= (WATCHDOG_TIMEOUT - threshold);
  }
  
  void pause() {
    if(!enabled) return;
    reset();
  }
  
  void resume() {
    reset();
  }
  
  bool isEnabled() {
    return enabled;
  }
  
  bool wasResetByWatchdog() {
    return wasWatchdogReset;
  }
};

// ============= MONITOR DE SALUD DEL SISTEMA =============
class SystemHealthMonitor {
private:
  struct HealthMetrics {
    unsigned long loopTime;        
    unsigned long maxLoopTime;     
    unsigned long i2cErrors;        
    unsigned long usbErrors;        
    unsigned long bufferOverflows;  
    unsigned long lastHealthCheck;  
  };
  
  HealthMetrics metrics;
  WatchdogManager* watchdog;
  
public:
  SystemHealthMonitor(WatchdogManager* wd) : watchdog(wd) {
    metrics.loopTime = 0;
    metrics.maxLoopTime = 0;
    metrics.i2cErrors = 0;
    metrics.usbErrors = 0;
    metrics.bufferOverflows = 0;
    metrics.lastHealthCheck = 0;
  }
  
  void updateLoopTime(unsigned long time) {
    metrics.loopTime = time;
    if(time > metrics.maxLoopTime) {
      metrics.maxLoopTime = time;
    }
    
    if(time > 100) {
      Serial.print("¡Loop lento detectado: ");
      Serial.print(time);
      Serial.println("ms!");
    }
  }
  
  void recordI2CError() {
    metrics.i2cErrors++;
    checkCriticalErrors();
  }
  
  void recordUSBError() {
    metrics.usbErrors++;
    checkCriticalErrors();
  }
  
  void recordBufferOverflow() {
    metrics.bufferOverflows++;
  }
  
  void checkCriticalErrors() {
    if(metrics.i2cErrors > 10) {
      Serial.println("¡Demasiados errores I2C! Considere reiniciar");
    }
  }
  
  void performHealthCheck() {
    unsigned long now = millis();
    
    if(now - metrics.lastHealthCheck < 1000) {
      return;
    }
    
    metrics.lastHealthCheck = now;
    
    if(metrics.loopTime < 50 && metrics.i2cErrors < 5) {
      watchdog->reset();
    }
    
    if(now % 60000 == 0) {
      metrics.i2cErrors = 0;
      metrics.usbErrors = 0;
      metrics.bufferOverflows = 0;
    }
  }
  
  void printMetrics() {
    Serial.println("=== METRICAS DE SALUD ===");
    Serial.print("Loop actual: ");
    Serial.print(metrics.loopTime);
    Serial.println("ms");
    Serial.print("Loop máximo: ");
    Serial.print(metrics.maxLoopTime);
    Serial.println("ms");
    Serial.print("Errores I2C: ");
    Serial.println(metrics.i2cErrors);
    Serial.print("Errores USB: ");
    Serial.println(metrics.usbErrors);
    Serial.print("Buffer overflows: ");
    Serial.println(metrics.bufferOverflows);
    Serial.print("Watchdog resets: ");
    
    unsigned long resets, lastReset;
    bool wasReset;
    watchdog->getStats(&resets, &lastReset, &wasReset);
    Serial.println(resets);
    
    Serial.println("========================");
  }
  
  void resetMetrics() {
    metrics.loopTime = 0;
    metrics.maxLoopTime = 0;
    metrics.i2cErrors = 0;
    metrics.usbErrors = 0;
    metrics.bufferOverflows = 0;
  }
};

// ============= RECOVERY MANAGER =============
class RecoveryManager {
private:
  bool inRecoveryMode;
  unsigned long recoveryStartTime;
  uint8_t recoveryAttempts;
  
public:
  RecoveryManager() {
    inRecoveryMode = false;
    recoveryStartTime = 0;
    recoveryAttempts = 0;
  }
  
  void enterRecoveryMode() {
    if(!inRecoveryMode) {
      inRecoveryMode = true;
      recoveryStartTime = millis();
      recoveryAttempts++;
      
      Serial.println("=== MODO RECUPERACION ACTIVADO ===");
      Serial.print("Intento #");
      Serial.println(recoveryAttempts);
    }
  }
  
  bool attemptRecovery() {
    if(!inRecoveryMode) return true;
    
    Serial.println("Intentando recuperacion...");
    
    Wire.end();
    delay(100);
    Wire.begin();
    
    unsigned long recoveryTime = millis() - recoveryStartTime;
    if(recoveryTime > 5000) {
      Serial.println("Recuperacion fallida - reiniciando sistema");
      NVIC_SystemReset();
    }
    
    return false;
  }
  
  void exitRecoveryMode() {
    if(inRecoveryMode) {
      inRecoveryMode = false;
      Serial.println("=== SISTEMA RECUPERADO ===");
    }
  }
  
  bool isInRecovery() {
    return inRecoveryMode;
  }
};

#endif