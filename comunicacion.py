import serial
import time

class ComunicacionSerial:
    def __init__(self, puerto="COM5", baudios=115200):
        self.puerto = puerto
        self.baudios = baudios
        self.ser = serial.Serial(self.puerto, self.baudios, timeout=0.1)
        time.sleep(3)  # esperar boot del ESP32
        print(f"Conectado a {puerto} @ {baudios} bps")

    def leer_linea(self):
        """Lee una línea cruda del serial (texto)"""
        if self.ser.in_waiting:
            try:
                return self.ser.readline().decode(errors="ignore").strip()
            except:
                return None
        return None

    def leer_valor(self):
        """Lee una línea e intenta convertirla a float"""
        linea = self.leer_linea()
        if not linea:
            return None

        try:
            return float(linea)
        except ValueError:
            # No es número, se ignora silenciosamente
            return None

    def enviar(self, texto):
        if not texto.endswith("\n"):
            texto += "\n"
        self.ser.write(texto.encode())

    def cerrar(self):
        self.ser.close()
        print("Puerto cerrado")
