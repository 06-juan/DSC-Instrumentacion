#include "DHT.h"

#define DHTPIN 15        // Pin donde está conectado el DHT22
#define DHTTYPE DHT22   // Tipo de sensor

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  dht.begin();
}

void loop() {
  delay(250); // El DHT22 necesita al menos 2 segundos entre lecturas

  float humedad = dht.readHumidity();
  float temperatura = dht.readTemperature(); // Celsius

  // Verificar si la lectura falló
  if (isnan(humedad) || isnan(temperatura)) {
    Serial.println("Error al leer el DHT22");
    return;
  }

  Serial.println(temperatura);
}
