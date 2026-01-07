import serial
import time

class ComunicacionSerial:
    def __init__(self, puerto="COM5", baudios=115200):
        self.puerto = puerto
        self.baudios = baudios
        self.ser = serial.Serial(self.puerto, self.baudios, timeout=1)
        time.sleep(3)  # esperar boot del ESP32

    def leer_valor(self):
        linea = self.ser.readline().decode(errors="ignore").strip()
        if not linea:
            return None

        try:
            return float(linea)
        except ValueError:
            print("Ignorado:", linea)
            return None

    def cerrar(self):
        self.ser.close()
        print("Puerto cerrado")
