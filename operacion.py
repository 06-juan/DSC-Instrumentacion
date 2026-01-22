import time
import csv
import os

class OperadorDiferencia:
    def __init__(self, archivo="datos_serial.csv"):
        self.t0 = None
        self.nombre_archivo = archivo
        self.f = None
        self.writer = None
        # Preparamos el archivo al instanciar
        self.abrir_archivo()

    def abrir_archivo(self):
        """Crea o sobreescribe el archivo CSV y escribe la cabecera"""
        # Si ya había un archivo abierto, lo cerramos
        self.cerrar()
        
        # Abrimos el archivo (modo 'w' sobreescribe el anterior)
        self.f = open(self.nombre_archivo, "w", newline="")
        self.writer = csv.writer(self.f)
        self.writer.writerow(["tiempo_s", "temp1", "temp2", "diferencia"])
        self.f.flush()

    def procesar(self, t1, t2):
        if self.t0 is None:
            self.t0 = time.time()

        t = time.time() - self.t0
        diff = t1 - t2 

        # Guardar en CSV solo si el archivo está abierto
        if self.writer:
            self.writer.writerow([f"{t:.3f}", f"{t1:.3f}", f"{t2:.3f}", f"{diff:.3f}"])
            self.f.flush()

        return t, diff

    def cerrar(self):
        """Cierra el archivo de forma segura"""
        if self.f:
            self.f.close()
            self.f = None
            self.writer = None