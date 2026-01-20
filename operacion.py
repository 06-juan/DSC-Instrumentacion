import time
import csv
import os

class OperadorDiferencia:
    def __init__(self, archivo="datos_serial.csv"):
        self.t0 = None
        self.archivo = archivo

        if os.path.exists(self.archivo):
            os.remove(self.archivo)
        
        self.f = open(self.archivo, "w", newline="")
        self.writer = csv.writer(self.f)
        # Nueva cabecera con ambas temperaturas
        self.writer.writerow(["tiempo_s", "temp1", "temp2", "diferencia"])
        self.f.flush()

    def procesar(self, t1, t2):
        if self.t0 is None:
            self.t0 = time.time()

        t = time.time() - self.t0
        diff = t1 - t2 # Diferencia entre los dos sensores

        # Guardar en CSV
        self.writer.writerow([f"{t:.3f}", f"{t1:.3f}", f"{t2:.3f}", f"{diff:.3f}"])
        self.f.flush()

        return t, diff

    def cerrar(self):
        self.f.close()