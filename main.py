from comunicacion import ComunicacionSerial
from operacion import OperadorDiferencia
from graficacion import GraficadorTiempoReal

import time

com = ComunicacionSerial(puerto="COM5", baudios=115200)
op = OperadorDiferencia()
graf = GraficadorTiempoReal(ventana=100)

print("Sistema iniciado. Ctrl+C para salir.")

try:
    while True:
        valor = com.leer_valor()
        if valor is None:
            continue

        t, diff = op.procesar(valor)
        if t is None:
            continue

        graf.actualizar(t, diff)
        #print(f"t = {t:.3f} s | Δ = {diff:.6f}")

except KeyboardInterrupt:
    print("\nInterrumpido por el usuario")

finally:
    com.cerrar()
    graf.mostrar()
