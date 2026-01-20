from comunicacion import ComunicacionSerial
from operacion import OperadorDiferencia
from graficacion import GraficadorTiempoReal
from pyqtgraph.Qt import QtCore
import time

n = 100  # segundos de medición

com = ComunicacionSerial(puerto="COM5", baudios=115200)
op = OperadorDiferencia()
graf = GraficadorTiempoReal(n)

estado = "idle"   # idle | midiendo
t_inicio = None

print("Sistema iniciado. Esperando ping del ESP32...")

# -------------------------
# TICK PRINCIPAL (Qt manda)
# -------------------------
def tick():
    global estado, t_inicio

    linea = com.leer_linea()

    # ==============================
    # 1. GESTIÓN DE TIEMPO
    # ==============================
    if estado == "midiendo":
        if time.time() - t_inicio >= n:
            print("⏰ Tiempo cumplido, enviando STOP")
            com.enviar("stop")
            estado = "idle"
            t_inicio = None
            return

    # ==============================
    # 2. SI NO LLEGA NADA, SALIMOS
    # ==============================
    if linea is None:
        return

    linea = linea.strip().lower()

    # ==============================
    # 3. PROTOCOLO PING / PONG
    # ==============================
    if linea == "ping" and estado == "idle":
        print(">> Ping recibido")
        com.enviar(f"pong {n}")
        print(f"<< Pong enviado con n = {n}")
        t_inicio = time.time()
        estado = "midiendo"
        graf.limpiar()
        return

    if linea == "pong" and estado == "idle":
        print("<< Pong recibido, medición detenida")
        t_inicio = None
        return

    # ==============================
    # 4. PROCESAMIENTO DE DATOS
    # ==============================
    if estado == "midiendo":
        try:
            # Suponiendo que llega "25.0,24.5"
            valores = linea.split(",")
            t1 = float(valores[0])
            t2 = float(valores[1])
        except (ValueError, IndexError):
            return

        # Procesamos enviando ambos valores
        t, diff = op.procesar(t1, t2)
        
        if t is None:
            return

        # Actualizamos la gráfica (usamos t1 como referencia visual de temp)
        graf.actualizar(t, diff, t1, t2)


# -------------------------
# QT TIMER (el corazón)
# -------------------------
timer = QtCore.QTimer()
timer.timeout.connect(tick)
timer.start(10)   # cada 10 ms → 100 Hz de refresco


# -------------------------
# ARRANQUE DE LA APP
# -------------------------
if __name__ == "__main__":
    try:
        graf.app.exec()
    finally:
        com.cerrar()
        print("Sistema cerrado limpio.")
