#include <SPI.h>
#include <TFT_eSPI.h>
#include <math.h>
#include "FS.h"

// --- TUS PINOS ACTUALES ---
#define HSPI_SCLK  33
#define HSPI_MISO  26
#define HSPI_MOSI  25
#define HSPI_CS    32

// --- Comandos ADS1220 ---
#define ADS_CMD_RESET  0x06
#define ADS_CMD_START  0x08

const float RES_NOMINAL = 10000.0, TEMP_NOMINAL = 298.15, BETA_COEFF = 3950.0;
const float IDAC_VALUE = 0.000100, K_SENSITIVITY = 0.00004127, VOLTAGE_REF = 2.048, GAIN = 128.0;

// Inicializamos el bus HSPI
SPIClass hspi(HSPI); 
// De 1MHz a 500kHz
SPISettings adsSettings(500000, MSBFIRST, SPI_MODE1);

TFT_eSPI tft = TFT_eSPI(); 

volatile bool midiendo = false;
enum ScreenState { SCREEN_HOME, SCREEN_MEASURE_WAIT, SCREEN_MEASURING, SCREEN_ERROR, SCREEN_CALIBRATE };
ScreenState currentScreen = SCREEN_HOME;

int btnW, btnH;
unsigned long measureStart = 0, measureDuration = 0;
int remainingSeconds = 0;

TaskHandle_t TareaADC;
TaskHandle_t TareaInterfaz;

// ------------------ SETUP ------------------
void setup() {
  Serial.begin(115200);

  // 1. Inicializar Pantalla (Usa VSPI por defecto)
  tft.init();
  tft.setRotation(1);
  btnW = tft.width() / 2;
  btnH = tft.height();
  drawHome();

  // 2. Inicializar ADS1220 (Tu bus HSPI personalizado)
  pinMode(HSPI_CS, OUTPUT);
  digitalWrite(HSPI_CS, HIGH);
  
  // IMPORTANTE: Esta línea mapea el hardware HSPI a tus pines específicos
  hspi.begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_CS);

  ads_reset();
  delay(100);

  // Crear tareas en núcleos separados
  xTaskCreatePinnedToCore(taskInterfaz, "TareaInterfaz", 8192, NULL, 1, &TareaInterfaz, 1);
  xTaskCreatePinnedToCore(loopLectura, "TareaADC", 8192, NULL, 2, &TareaADC, 0);
}

void loop() {
  // El loop principal no hace nada, todo está en las tareas
  vTaskDelay(portMAX_DELAY);
}

// ------------------ NÚCLEO 0: MEDICIÓN (HSPI) ------------------
void loopLectura(void * pvParameters) {
  for (;;) {
    if (midiendo) {
      // 1. Leer CJC
      config_ads_ntc(); 
      ads_start(); 
      vTaskDelay(pdMS_TO_TICKS(25)); 
      long rawNTC = wait_and_read();
      
      float vNTC = (float)rawNTC * (VOLTAGE_REF / 8388607.0);
      float rNTC = vNTC / IDAC_VALUE;
      float tempCJC = (1.0 / ((log(rNTC / RES_NOMINAL) / BETA_COEFF) + (1.0 / TEMP_NOMINAL))) - 273.15;

      // 2. Leer TC1
      config_ads_tc(0x0E); 
      ads_start(); 
      vTaskDelay(pdMS_TO_TICKS(25)); 
      long rawTC1 = wait_and_read();
      float vTC1 = (float)rawTC1 * (VOLTAGE_REF / (8388607.0 * GAIN));
      float t1 = (vTC1 / K_SENSITIVITY) + tempCJC;

      // 3. Leer TC2
      config_ads_tc(0x5E); 
      ads_start(); 
      vTaskDelay(pdMS_TO_TICKS(25)); 
      long rawTC2 = wait_and_read();
      float vTC2 = (float)rawTC2 * (VOLTAGE_REF / (8388607.0 * GAIN));
      float t2 = (vTC2 / K_SENSITIVITY) + tempCJC;

      // Formato para Python: t1,t2
      Serial.print(t1, 2); 
      Serial.print(","); 
      Serial.println(t2, 2);

      vTaskDelay(pdMS_TO_TICKS(50)); 
    } else {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

// --- FUNCIONES SPI PARA HSPI ---
void config_ads_ntc() {
  hspi.beginTransaction(adsSettings);
  digitalWrite(HSPI_CS, LOW);
  hspi.transfer(0x43); hspi.transfer(0xB1); hspi.transfer(0x04); hspi.transfer(0x13); hspi.transfer(0x82);
  digitalWrite(HSPI_CS, HIGH);
  hspi.endTransaction();
}

void config_ads_tc(uint8_t mux_gain_byte) {
  hspi.beginTransaction(adsSettings);
  digitalWrite(HSPI_CS, LOW);
  hspi.transfer(0x43); hspi.transfer(mux_gain_byte); hspi.transfer(0x04); hspi.transfer(0x10); hspi.transfer(0x02);
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
  // Importante: Checar el pin MISO configurado (26)
  while (digitalRead(HSPI_MISO) == HIGH) {
    if (millis() - timeout > 300) { 
      // Si falla, intentamos resetear el chip por software
      ads_reset(); 
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

// (Funciones de UI idénticas al anterior...)
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
  if (currentScreen == SCREEN_HOME) { if (x < btnW) startPing(); else drawCalibrate(); }
  else if (currentScreen == SCREEN_ERROR) startPing();
  else if (currentScreen == SCREEN_CALIBRATE) drawHome();
}