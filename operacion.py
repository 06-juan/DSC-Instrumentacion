import time
import csv
import os

class OperadorDiferencia:
    def __init__(self, archivo="datos_serial.csv"):
        self.valor_anterior = None
        self.t0 = None
        self.archivo = archivo

        # --- borrar archivo anterior si existe ---
        if os.path.exists(self.archivo):
            os.remove(self.archivo)
            print(f"Archivo anterior '{self.archivo}' eliminado")

        # --- crear archivo nuevo ---
        self.f = open(self.archivo, "w", newline="")
        self.writer = csv.writer(self.f)
        self.writer.writerow(["tiempo_s", "valor", "diferencia"])
        self.f.flush()

        print(f"Nuevo registro iniciado en {self.archivo}")

    def procesar(self, valor_actual):
        if self.t0 is None:
            self.t0 = time.time()
            self.valor_anterior = valor_actual
            return None, None  # todavía no hay diferencia

        t = time.time() - self.t0
        diff = valor_actual - self.valor_anterior
        self.valor_anterior = valor_actual

        # guardar en CSV
        # guardar en CSV con 3 decimales
        self.writer.writerow([f"{t:.3f}", f"{valor_actual:.3f}", f"{diff:.3f}"])
        self.f.flush()

        return t, diff

    def cerrar(self):
        self.f.close()
        print("Archivo de operación cerrado")
