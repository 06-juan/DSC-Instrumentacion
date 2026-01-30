#include <SPI.h>
#include <math.h>

// Pines ESP32
#define CS_PIN   32
#define SCLK_PIN 33
#define MOSI_PIN 25
#define MISO_PIN 26

// Comandos ADS1220
#define ADS_CMD_RESET  0x06
#define ADS_CMD_START  0x08

// Parámetros NTC y Termocupla
const float RES_NOMINAL = 10000.0;
const float TEMP_NOMINAL = 298.15;
const float BETA_COEFF = 3950.0;
const float IDAC_VALUE = 0.000100;
const float K_SENSITIVITY = 0.00004127; 
const float VOLTAGE_REF = 2.048;

SPISettings adsSettings(1000000, MSBFIRST, SPI_MODE1);

// Handle de la tarea
TaskHandle_t TareaADC;

void setup() {
  Serial.begin(115200);
  SPI.begin(SCLK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);

  ads_reset();
  delay(100);

  // Creamos la tarea en el NÚCLEO 0
  xTaskCreatePinnedToCore(
    loopLectura,    // Función de la tarea
    "TareaADC",     // Nombre
    10000,          // Stack size
    NULL,           // Parámetros
    1,              // Prioridad
    &TareaADC,      // Handle
    0               // <--- NÚCLEO 0
  );
}

void loop() {
  // El loop (Núcleo 1) queda libre para otras funciones
  vTaskDelay(portMAX_DELAY);
}

// ---------------------------------------------------------
// FUNCIÓN PRINCIPAL DE MEDICIÓN (NÚCLEO 0)
// ---------------------------------------------------------
void loopLectura(void * pvParameters) {
  for(;;) {
    // --- 1. LEER JUNTA FRÍA (NTC) ---
    config_ads_ntc();
    ads_start();
    vTaskDelay(pdMS_TO_TICKS(20)); 
    long rawNTC = wait_and_read();
    
    float vNTC = (float)rawNTC * (VOLTAGE_REF / 8388607.0);
    float rNTC = vNTC / IDAC_VALUE;
    float logR = log(rNTC / RES_NOMINAL);
    float tempCJC = (1.0 / ((logR / BETA_COEFF) + (1.0 / TEMP_NOMINAL))) - 273.15;

    // ---- 2.diff (AIN0-AIN1) ----
    config_tc_ain0_ain1();
    ads_start();
    delay(60);

    long rawdiff = wait_and_read();
    float GAIN_ADS_diff = 128.0; 
    float vdiff = rawdiff * (2.0 * VOLTAGE_REF) / (GAIN_ADS_diff * 8388608.0);
    float diff = (vdiff / K_SENSITIVITY);

    // ---- 3. TC1 (AIN0-AIN2) ----
    config_tc_ain0_ain2();
    ads_start();
    delay(60);

    long rawTC1 = wait_and_read();
    float GAIN_ADS1 = 32.0; 
    float vTC1 = rawTC1 * (2.0 * VOLTAGE_REF) / (GAIN_ADS1 * 8388608.0);
    float t1 = (vTC1 / K_SENSITIVITY) + tempCJC;

    // ---- 4. TC2 (AIN1-AIN2) ----
    config_tc_ain1_ain2();
    ads_start();
    delay(60);

    long rawTC2 = wait_and_read();
    float GAIN_ADS2 = 32.0; 
    float vTC2 = rawTC2 * (2.0 * VOLTAGE_REF) / (GAIN_ADS2 * 8388608.0);
    float t2 = (vTC2 / K_SENSITIVITY) + tempCJC;

// --- 5. IMPRESIÓN DE DATOS ---
    
    Serial.print(tempCJC, 4);
    Serial.print(" , ");
    Serial.print(t1, 4);
    Serial.print(" , ");
    Serial.print(t2, 4);
    Serial.print(" , ");
    Serial.println(diff, 4);

    vTaskDelay(pdMS_TO_TICKS(100)); // Frecuencia de muestreo (10 Hz)
  }
}

// ---------------------------------------------------------
// FUNCIONES ADS1220 (Sin cambios sustanciales)
// ---------------------------------------------------------

void config_ads_ntc() {
  SPI.beginTransaction(adsSettings);
  digitalWrite(CS_PIN, LOW);
  SPI.transfer(0x43); 
  SPI.transfer(0xB1); 
  SPI.transfer(0x04); 
  SPI.transfer(0x13); 
  SPI.transfer(0x82); 
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();
}

void config_ads_tc(uint8_t mux_gain_byte) {
  SPI.beginTransaction(adsSettings);
  digitalWrite(CS_PIN, LOW);
  SPI.transfer(0x43); 
  SPI.transfer(mux_gain_byte); 
  SPI.transfer(0x04); 
  SPI.transfer(0x10); 
  SPI.transfer(0x02); 
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();
}

void ads_reset() {
  SPI.beginTransaction(adsSettings);
  digitalWrite(CS_PIN, LOW);
  SPI.transfer(ADS_CMD_RESET);
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();
}

void ads_start() {
  SPI.beginTransaction(adsSettings);
  digitalWrite(CS_PIN, LOW);
  SPI.transfer(ADS_CMD_START);
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();
}

long wait_and_read() {
  unsigned long timeout = millis();
  while (digitalRead(MISO_PIN) == HIGH) {
    if (millis() - timeout > 500) return 0;
  }
  
  digitalWrite(CS_PIN, LOW);
  SPI.beginTransaction(adsSettings);
  uint8_t b1 = SPI.transfer(0x00);
  uint8_t b2 = SPI.transfer(0x00);
  uint8_t b3 = SPI.transfer(0x00);
  SPI.endTransaction();
  digitalWrite(CS_PIN, HIGH);
  
  long value = ((long)b1 << 16) | ((long)b2 << 8) | b3;
  if (value & 0x800000) value |= 0xFF000000;
  return value;
}

void config_tc_ain0_ain1() {
  uint8_t r[4] = {
    0x0E,        // AIN0-AIN1, PGA=128
    0x04,        // 20 SPS, single-shot
    0x20,        // Vref interna
    0x00
  };
  write_regs(r);
}

void config_tc_ain0_ain2() {
  uint8_t r[4] = {
    0x1A,        // AIN0-AIN2, PGA=32
    0x04,
    0x20,
    0x00
  };
  write_regs(r);
}

void config_tc_ain1_ain2() {
  uint8_t r[4] = {
    0x3A,        // AIN1-AIN2, PGA=32 (Cambio de 0x1A a 0x3A)
    0x04,        // Configuración de velocidad/modo
    0x20,        // Referencia de voltaje
    0x00         // IDACs y otros
  };
  write_regs(r);
}

void write_regs(uint8_t *r) {
  SPI.beginTransaction(adsSettings);
  digitalWrite(CS_PIN, LOW);
  SPI.transfer(0x40 | 0x03);
  for (int i = 0; i < 4; i++) SPI.transfer(r[i]);
  digitalWrite(CS_PIN, HIGH);
  SPI.endTransaction();
}

