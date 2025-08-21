#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <IWatchdog.h>

// ============= CONFIGURACIÓN WATCHDOG =============
#define WATCHDOG_TIMEOUT 5000     // 5 segundos timeout
#define WATCHDOG_ENABLED true     // Habilitar/deshabilitar watchdog

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
  
  // Inicializar watchdog
  bool init() {
    if(!enabled) {
      Serial.println("Watchdog deshabilitado");
      return false;
    }
    
    // Verificar si el último reset fue por watchdog
    wasWatchdogReset = IWatchdog.isReset();
    
    if(wasWatchdogReset) {
      Serial.println("¡ALERTA! Sistema reiniciado por Watchdog");
      // Aquí podrías guardar info de debug en EEPROM
    }
    
    // Configurar y activar watchdog
    if(IWatchdog.isSupported()) {
      // STM32 soporta hasta 32 segundos
      IWatchdog.begin(WATCHDOG_TIMEOUT * 1000);  // Convertir a microsegundos
      
      Serial.print("Watchdog inicializado: ");
      Serial.print(WATCHDOG_TIMEOUT / 1000);
      Serial.println(" segundos");
      
      lastReset = millis();
      return true;
    } else {
      Serial.println("Watchdog no soportado en este hardware");
      enabled = false;
      return false;
    }
  }
  
  // Resetear el contador del watchdog
  void reset() {
    if(!enabled) return;
    
    IWatchdog.reload();
    lastReset = millis();
    resetCount++;
  }
  
  // Reset condicional - solo si pasó cierto tiempo
  void conditionalReset(unsigned long minInterval = 100) {
    if(!enabled) return;
    
    unsigned long now = millis();
    if(now - lastReset >= minInterval) {
      reset();
    }
  }
  
  // Obtener tiempo desde último reset
  unsigned long getTimeSinceReset() {
    return millis() - lastReset;
  }
  
  // Obtener estadísticas
  void getStats(unsigned long* resets, unsigned long* lastResetTime, bool* wasReset) {
    *resets = resetCount;
    *lastResetTime = lastReset;
    *wasReset = wasWatchdogReset;
  }
  
  // Verificar si está cerca del timeout (para debug)
  bool isNearTimeout(unsigned long threshold = 1000) {
    if(!enabled) return false;
    
    unsigned long timeSinceReset = getTimeSinceReset();
    return timeSinceReset >= (WATCHDOG_TIMEOUT - threshold);
  }
  
  // Deshabilitar temporalmente (para operaciones largas)
  void pause() {
    if(!enabled) return;
    
    // En STM32, no se puede pausar, solo resetear más frecuentemente
    reset();
  }
  
  // Reanudar (compatibilidad)
  void resume() {
    reset();
  }
  
  // Estado del watchdog
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
    unsigned long loopTime;        // Tiempo del loop principal
    unsigned long maxLoopTime;     // Máximo tiempo registrado
    unsigned long i2cErrors;        // Errores de comunicación I2C
    unsigned long usbErrors;        // Errores USB
    unsigned long bufferOverflows;  // Desbordamientos de buffer
    unsigned long lastHealthCheck;  // Última verificación
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
  
  // Actualizar tiempo de loop
  void updateLoopTime(unsigned long time) {
    metrics.loopTime = time;
    if(time > metrics.maxLoopTime) {
      metrics.maxLoopTime = time;
    }
    
    // Si el loop toma mucho tiempo, alertar
    if(time > 100) {  // más de 100ms es problemático
      Serial.print("¡Loop lento detectado: ");
      Serial.print(time);
      Serial.println("ms!");
    }
  }
  
  // Registrar error I2C
  void recordI2CError() {
    metrics.i2cErrors++;
    checkCriticalErrors();
  }
  
  // Registrar error USB
  void recordUSBError() {
    metrics.usbErrors++;
    checkCriticalErrors();
  }
  
  // Registrar overflow de buffer
  void recordBufferOverflow() {
    metrics.bufferOverflows++;
  }
  
  // Verificar errores críticos
  void checkCriticalErrors() {
    // Si hay muchos errores seguidos, podría ser necesario reiniciar
    if(metrics.i2cErrors > 10) {
      Serial.println("¡Demasiados errores I2C! Considere reiniciar");
      // Aquí podrías forzar un reinicio dejando que el watchdog expire
    }
  }
  
  // Verificación periódica de salud
  void performHealthCheck() {
    unsigned long now = millis();
    
    // Verificar cada segundo
    if(now - metrics.lastHealthCheck < 1000) {
      return;
    }
    
    metrics.lastHealthCheck = now;
    
    // Reset watchdog si todo está bien
    if(metrics.loopTime < 50 && metrics.i2cErrors < 5) {
      watchdog->reset();
    }
    
    // Limpiar contadores periódicamente
    if(now % 60000 == 0) {  // Cada minuto
      metrics.i2cErrors = 0;
      metrics.usbErrors = 0;
      metrics.bufferOverflows = 0;
    }
  }
  
  // Obtener métricas para debug
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
  
  // Reset métricas
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
  
  // Entrar en modo recuperación
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
  
  // Intentar recuperación
  bool attemptRecovery() {
    if(!inRecoveryMode) return true;
    
    Serial.println("Intentando recuperacion...");
    
    // Reiniciar comunicaciones
    Wire.end();
    delay(100);
    Wire.begin();
    
    // Verificar si mejoró
    // Aquí deberías verificar el hardware problemático
    
    unsigned long recoveryTime = millis() - recoveryStartTime;
    if(recoveryTime > 5000) {  // 5 segundos máximo
      Serial.println("Recuperacion fallida - reiniciando sistema");
      NVIC_SystemReset();  // Reset por software
    }
    
    return false;
  }
  
  // Salir del modo recuperación
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