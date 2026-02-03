#include <SPI.h>
#include <TFT_eSPI.h>
#include <math.h>
#include "FS.h"

// --- PINES PANTALLA Y TÁCTIL (VSPI - Definidos en User_Setup.h) ---
// TFT_DC 4, TFT_CS 15, TFT_MOSI 16, TFT_MISO 5, TFT_SCLK 17, TFT_RST 2, TOUCH_CS 18

// --- PINES ADS1220 (HSPI) ---
#define HSPI_SCLK  33
#define HSPI_MISO  26
#define HSPI_MOSI  25
#define HSPI_CS    32

// --- Comandos ADS1220 ---
#define ADS_CMD_RESET  0x06
#define ADS_CMD_START  0x08

// Parámetros Físicos
const float RES_NOMINAL = 10000.0;
const float TEMP_NOMINAL = 298.15;
const float BETA_COEFF = 3950.0;
const float IDAC_VALUE = 0.000100;
const float K_SENSITIVITY = 0.00004127; 
const float VOLTAGE_REF = 2.048;

// Nota: Eliminamos la GAIN global para evitar errores, se definirá localmente.

// Inicializamos el bus HSPI
SPIClass hspi(HSPI); 
SPISettings adsSettings(1000000, MSBFIRST, SPI_MODE1); // Subido a 1MHz como en tu código original

TFT_eSPI tft = TFT_eSPI(); 

volatile bool midiendo = false;
enum ScreenState { SCREEN_HOME, SCREEN_MEASURE_WAIT, SCREEN_MEASURING, SCREEN_ERROR, SCREEN_CALIBRATE };
ScreenState currentScreen = SCREEN_HOME;

int btnW, btnH;
unsigned long measureStart = 0, measureDuration = 0;
int remainingSeconds = 0;

TaskHandle_t TareaADC;
TaskHandle_t TareaInterfaz;

// Prototipos de funciones auxiliares para evitar errores de compilación
void config_ads_ntc();
void config_ads_tc_general(uint8_t mux, uint8_t gain_reg); // Función genérica unificada
void ads_reset();
void ads_start();
long wait_and_read();
void drawHome();
void drawWaiting();
void drawMeasuring();
void drawError();
void drawCalibrate();
void handleTouch(uint16_t x, uint16_t y);
void startPing();
void checkForPong();
void updateMeasuringUI();
void checkForStop();
void loopLectura(void * pvParameters);
void taskInterfaz(void * pvParameters);


// ------------------ SETUP ------------------
void setup() {
  Serial.begin(115200);

  // 1. Inicializar Pantalla (Usa VSPI por defecto)
  tft.init();
  tft.setRotation(1);
  btnW = tft.width() / 2;
  btnH = tft.height();
  drawHome();

  // 2. Inicializar ADS1220 (HSPI)
  pinMode(HSPI_CS, OUTPUT);
  digitalWrite(HSPI_CS, HIGH);
  
  hspi.begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_CS);

  ads_reset();
  delay(100);

  // Crear tareas
  // Interfaz en Core 1 (Arduino Loop default)
  xTaskCreatePinnedToCore(taskInterfaz, "TareaInterfaz", 8192, NULL, 1, &TareaInterfaz, 1);
  // ADC en Core 0 (Dedicado)
  xTaskCreatePinnedToCore(loopLectura, "TareaADC", 10000, NULL, 2, &TareaADC, 0);
}

void loop() {
  vTaskDelay(portMAX_DELAY); // El loop principal cede el control
}

// ------------------ NÚCLEO 0: MEDICIÓN (HSPI) ------------------
void loopLectura(void * pvParameters) {
  for (;;) {
    if (midiendo) {
      // --- 1. LEER JUNTA FRÍA (NTC) ---
      config_ads_ntc();
      ads_start();
      vTaskDelay(pdMS_TO_TICKS(20)); // delay no bloqueante
      long rawNTC = wait_and_read();
      
      float vNTC = (float)rawNTC * (VOLTAGE_REF / 8388607.0);
      float rNTC = vNTC / IDAC_VALUE;
      // Evitar log(0) o valores negativos si no hay sensor
      if (rNTC <= 0) rNTC = RES_NOMINAL; 
      float logR = log(rNTC / RES_NOMINAL);
      float tempCJC = (1.0 / ((logR / BETA_COEFF) + (1.0 / TEMP_NOMINAL))) - 273.15;

      // --- 2. diff (AIN0-AIN1) ---
      // Config: AIN0-AIN1 (0x00) + PGA=128 (0x0E) -> Registro 0x0E
      // El código original usaba write_regs con 0x0E.
      config_tc_ain0_ain1(); // 0x0E es el registro 0 completo (MUX + GAIN)
      ads_start();
      vTaskDelay(pdMS_TO_TICKS(60)); // Tiempo de conversión

      long rawdiff = wait_and_read();
      float GAIN_ADS_diff = 32; 
      float vdiff = rawdiff * (2.0 * VOLTAGE_REF) / (GAIN_ADS_diff * 8388608.0);
      float diff = (vdiff / K_SENSITIVITY);

      // --- 3. TC1 (AIN0-AIN2) ---
      // Config: AIN0-AIN2 (0x01) + PGA=32 (0x0A) -> Registro 0x1A
      config_tc_ain0_ain2();
      ads_start();
      vTaskDelay(pdMS_TO_TICKS(60));

      long rawTC1 = wait_and_read();
      float GAIN_ADS1 = 32.0; 
      float vTC1 = rawTC1 * (2.0 * VOLTAGE_REF) / (GAIN_ADS1 * 8388608.0);
      float t1 = (vTC1 / K_SENSITIVITY) + tempCJC;

      // --- 4. TC2 (AIN1-AIN2) ---
      // Config: AIN1-AIN2 (0x03) + PGA=32 (0x0A) -> Registro 0x3A
      config_tc_ain1_ain2();
      ads_start();
      vTaskDelay(pdMS_TO_TICKS(60));

      long rawTC2 = wait_and_read();
      float GAIN_ADS2 = 32.0; 
      float vTC2 = rawTC2 * (2.0 * VOLTAGE_REF) / (GAIN_ADS2 * 8388608.0);
      float t2 = (vTC2 / K_SENSITIVITY) + tempCJC;

      // --- 5. IMPRESIÓN DE DATOS (Formato Original) ---
      Serial.print(tempCJC, 4);
      Serial.print(" , ");
      Serial.print(t1, 4);
      Serial.print(" , ");
      Serial.print(t2, 4);
      Serial.print(" , ");
      Serial.println(diff, 4);

      // Frecuencia global del ciclo
      vTaskDelay(pdMS_TO_TICKS(50)); 
    } else {
      // Si no estamos midiendo, dormimos la tarea para ahorrar CPU
      vTaskDelay(pdMS_TO_TICKS(200));
    }
  }
}

// --- FUNCIONES SPI PARA HSPI ---

void config_ads_ntc() {
  hspi.beginTransaction(adsSettings);
  digitalWrite(HSPI_CS, LOW);
  hspi.transfer(0x43); 
  hspi.transfer(0xB1); 
  hspi.transfer(0x04); 
  hspi.transfer(0x13); 
  hspi.transfer(0x82); 
  digitalWrite(HSPI_CS, HIGH);
  hspi.endTransaction();
}

// Función genérica para configurar cualquier termopar
// mux_gain: el byte que calculamos antes (ej: 0x0A para PGA 32)
void config_tc_generic(uint8_t mux_gain) {
  // 1. Cambiamos Canal y Ganancia (Registro 0)
  write_single_reg(0, mux_gain);
  
  // 2. IMPORTANTE: Apagamos IDACs que pudieron quedar de la NTC (Registro 2)
  // Si no haces esto, la corriente del NTC afectará la lectura del termopar.
  write_single_reg(2, 0x00); 
}

void config_tc_ain0_ain1() { config_tc_generic(0x0A); } // AIN0-AIN1, Ganancia 32
void config_tc_ain0_ain2() { config_tc_generic(0x1A); } // AIN0-AIN2, Ganancia 32
void config_tc_ain1_ain2() { config_tc_generic(0x3A); } // AIN1-AIN2, Ganancia 32

void write_single_reg(uint8_t regAddr, uint8_t value) {
  hspi.beginTransaction(adsSettings);
  digitalWrite(HSPI_CS, LOW);
  
  // Comando: 0x40 (WREG) | (regAddr << 2) | (numero_de_regs_menos_1)
  // Para un solo registro, nn es 0.
  hspi.transfer(0x40 | (regAddr << 2)); 
  hspi.transfer(value);
  
  digitalWrite(HSPI_CS, HIGH);
  hspi.endTransaction();
}

void ads_reset() {
  hspi.beginTransaction(adsSettings);
  digitalWrite(HSPI_CS, LOW);
  hspi.transfer(ADS_CMD_RESET);
  digitalWrite(HSPI_CS, HIGH);
  hspi.endTransaction();
}

void ads_start() {
  hspi.beginTransaction(adsSettings);
  digitalWrite(HSPI_CS, LOW);
  hspi.transfer(ADS_CMD_START);
  digitalWrite(HSPI_CS, HIGH);
  hspi.endTransaction();
}

long wait_and_read() {
  unsigned long timeout = millis();
  // Chequeamos MISO (Pin 26)
  while (digitalRead(HSPI_MISO) == HIGH) {
    if (millis() - timeout > 500) {
        // Timeout de seguridad
        return 0; 
    }
  }
  
  digitalWrite(HSPI_CS, LOW);
  hspi.beginTransaction(adsSettings);
  uint8_t b1 = hspi.transfer(0x00);
  uint8_t b2 = hspi.transfer(0x00);
  uint8_t b3 = hspi.transfer(0x00);
  hspi.endTransaction();
  digitalWrite(HSPI_CS, HIGH);
  
  long value = ((long)b1 << 16) | ((long)b2 << 8) | b3;
  if (value & 0x800000) value |= 0xFF000000;
  return value;
}

// ------------------ NÚCLEO 1: INTERFAZ ------------------
void taskInterfaz(void * pvParameters) {
  for (;;) {
    uint16_t x, y;
    // TFT_eSPI gestiona su propio bus VSPI
    if (tft.getTouch(&x, &y)) {
      handleTouch(x, y);
      vTaskDelay(pdMS_TO_TICKS(200));
    }
    if (currentScreen == SCREEN_MEASURE_WAIT) checkForPong();
    if (currentScreen == SCREEN_MEASURING) updateMeasuringUI();
    checkForStop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// --- FUNCIONES UI (Misma lógica anterior) ---
void startPing() { drawWaiting(); Serial.println("ping"); }

void checkForPong() {
  static unsigned long startWait = millis();
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n'); line.trim();
    if (line.startsWith("pong")) {
      int n = line.substring(4).toInt();
      if (n > 0) {
        measureDuration = n * 1000UL; measureStart = millis(); remainingSeconds = n;
        drawMeasuring(); midiendo = true; startWait = millis(); return;
      }
    }
  }
  if (millis() - startWait > 3000) { drawError(); startWait = millis(); }
}

void checkForStop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n'); line.trim();
    if (line == "stop") { midiendo = false; if (currentScreen == SCREEN_MEASURING) { Serial.println("pong"); drawHome(); } }
  }
}

void updateMeasuringUI() {
  unsigned long elapsed = millis() - measureStart;
  int newRemaining = (int)((measureDuration - elapsed) / 1000);
  if (newRemaining != remainingSeconds) {
    remainingSeconds = newRemaining;
    tft.fillRect(0, 0, tft.width(), 30, TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("Restante: " + String(remainingSeconds) + " s", 10, 10);
  }
  if (elapsed >= measureDuration) { midiendo = false; drawHome(); }
}

void drawHome() {
  currentScreen = SCREEN_HOME; tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, btnW, btnH, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE); tft.setTextDatum(MC_DATUM);
  tft.drawString("MEDIR", btnW / 2, btnH / 2);
  tft.fillRect(btnW, 0, btnW, btnH, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString("CALIBRAR", btnW + btnW / 2, btnH / 2);
}

void drawWaiting() {
  currentScreen = SCREEN_MEASURE_WAIT; tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextDatum(MC_DATUM);
  tft.drawString("Conectando...", tft.width()/2, tft.height()/2);
}

void drawMeasuring() {
  currentScreen = SCREEN_MEASURING; tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.setTextDatum(MC_DATUM);
  tft.drawString("Midiendo...", tft.width()/2, tft.height()/2 + 30);
}

void drawError() {
  currentScreen = SCREEN_ERROR; tft.fillScreen(TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED); tft.setTextDatum(MC_DATUM);
  tft.drawString("FALLO DE CONEXION", tft.width()/2, tft.height()/2 - 20);
  tft.drawString("REINTENTAR", tft.width()/2, tft.height()/2 + 20);
}

void drawCalibrate() {
  currentScreen = SCREEN_CALIBRATE; tft.fillScreen(TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY); tft.setTextDatum(MC_DATUM);
  tft.drawString("Menu de Calibracion", tft.width()/2, tft.height()/2);
}

void handleTouch(uint16_t x, uint16_t y) {
  if (currentScreen == SCREEN_HOME) { if (x > btnW) startPing(); else drawCalibrate(); }
  else if (currentScreen == SCREEN_ERROR) startPing();
  else if (currentScreen == SCREEN_CALIBRATE) drawHome();
}