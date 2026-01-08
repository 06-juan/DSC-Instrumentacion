#include <SPI.h>
#include <TFT_eSPI.h>
#include "FS.h"
#include "DHT.h"

#define DHTPIN 15
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);
TFT_eSPI tft = TFT_eSPI();

enum ScreenState {
  SCREEN_HOME,
  SCREEN_MEASURE_WAIT,
  SCREEN_MEASURING,
  SCREEN_ERROR,
  SCREEN_CALIBRATE
};

ScreenState currentScreen = SCREEN_HOME;

int btnW, btnH;

unsigned long measureStart = 0;
unsigned long measureDuration = 0;
int remainingSeconds = 0;

// ------------------ SETUP ------------------
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);

  touch_calibrate();

  btnW = tft.width() / 2;
  btnH = tft.height();

  drawHome();
  dht.begin();
}

// ------------------ LOOP ------------------
void loop() {
  uint16_t x, y;

  if (tft.getTouch(&x, &y)) {
    handleTouch(x, y);
    delay(200);
  }

  if (currentScreen == SCREEN_MEASURE_WAIT) {
    checkForPong();
  }

  if (currentScreen == SCREEN_MEASURING) {
    updateMeasuring();
  }

  checkForStop();   // <-- nuevo: siempre escuchamos stop
}

// ------------------ UI ------------------
void drawHome() {
  currentScreen = SCREEN_HOME;
  tft.fillScreen(TFT_BLACK);

  tft.fillRect(0, 0, btnW, btnH, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("MEDIR", btnW / 2, btnH / 2);

  tft.fillRect(btnW, 0, btnW, btnH, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString("CALIBRAR", btnW + btnW / 2, btnH / 2);
}

void drawWaiting() {
  currentScreen = SCREEN_MEASURE_WAIT;
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Conectando...", tft.width()/2, tft.height()/2);
}

void drawMeasuring() {
  currentScreen = SCREEN_MEASURING;
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Midiendo...", tft.width()/2, tft.height()/2 + 30);
}

void drawError() {
  currentScreen = SCREEN_ERROR;
  tft.fillScreen(TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("FALLO DE CONEXION", tft.width()/2, tft.height()/2 - 20);
  tft.drawString("REINTENTAR", tft.width()/2, tft.height()/2 + 20);
}

void drawCalibrate() {
  currentScreen = SCREEN_CALIBRATE;
  tft.fillScreen(TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Menu de Calibracion", tft.width()/2, tft.height()/2);
}

// ------------------ TOUCH ------------------
void handleTouch(uint16_t x, uint16_t y) {
  if (currentScreen == SCREEN_HOME) {
    if (x < btnW) {
      drawCalibrate();
    } else {
      startPing();
    }
  }

  else if (currentScreen == SCREEN_ERROR) {
    startPing();
  }

  else if (currentScreen == SCREEN_CALIBRATE) {
    drawHome();
  }
}

// ------------------ PING ------------------
void startPing() {
  drawWaiting();
  Serial.println("ping");
}

// ------------------ PONG ------------------
void checkForPong() {
  static unsigned long startWait = millis();

  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.startsWith("pong")) {
      int n = line.substring(4).toInt();

      if (n > 0) {
        measureDuration = n * 1000UL;
        measureStart = millis();
        remainingSeconds = n;
        drawMeasuring();
        startWait = millis();
        return;
      }
    }
  }

  if (millis() - startWait > 3000) {
    drawError();
    startWait = millis();
  }
}

// ------------------ STOP ------------------
void checkForStop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line == "stop" && currentScreen == SCREEN_MEASURING) {
      Serial.println("pong");   // confirmamos parada
      drawHome();
    }
  }
}

// ------------------ MEDICION ------------------
void updateMeasuring() {
  unsigned long elapsed = millis() - measureStart;

  float humedad = dht.readHumidity();
  float temperatura = dht.readTemperature();

  if (!isnan(temperatura)) {
    Serial.println(temperatura);
  }

  int newRemaining = (measureDuration - elapsed) / 1000;

  if (newRemaining != remainingSeconds) {
    remainingSeconds = newRemaining;

    tft.fillRect(0, 0, tft.width(), 30, TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("Restante: " + String(remainingSeconds) + " s", 10, 10);
  }
}

// ------------------ CALIBRACION ------------------
void touch_calibrate() {
  // aun no hace nada
}
