from comunicacion import ComunicacionSerial
from operacion import OperadorDiferencia
from graficacion import GraficadorTiempoReal
from pyqtgraph.Qt import QtCore
import time

n = 100  # segundos de medición
offset = 0.68

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
    global estado, t_inicio, op

    linea = com.leer_linea()

    # ==============================
    # 1. GESTIÓN DE TIEMPO (Fin de medición)
    # ==============================
    if estado == "midiendo":
        if time.time() - t_inicio >= n+13:
            print("⏰ Tiempo cumplido, enviando STOP")
            com.enviar("stop")
            estado = "idle"
            t_inicio = None
            # No retornamos aquí para permitir leer el buffer si quedó algo
    
    # ==============================
    # 2. SI NO LLEGA NADA, SALIMOS
    # ==============================
    if linea is None:
        return

    linea = linea.strip().lower()

    # ==============================
    # 3. PROTOCOLO PING / PONG (Reinicio de sesión)
    # ==============================
    # Si llega un ping y estamos en idle, iniciamos una nueva medición
    if linea == "ping" and estado == "idle":
        print("\n>> NUEVA MEDICIÓN INICIADA")
        print(">> Ping recibido")
        
        # REINICIO CRÍTICO:
        op = OperadorDiferencia() # Reiniciamos el operador para que el tiempo T empiece en 0
        graf.limpiar()            # Borramos la gráfica anterior
        
        com.enviar(f"pong {n}")
        print(f"<< Pong enviado con n = {n+13}")
        
        t_inicio = time.time()
        estado = "midiendo"
        return

    # Si el ESP32 confirma que se detuvo
    if linea == "pong" and estado == "idle":
        print("<< Confirmación de parada recibida (ESP32 en modo Home)")
        return

    # ==============================
    # 4. PROCESAMIENTO DE DATOS
    # ==============================
    if estado == "midiendo":
        try:
            valores = linea.split(",")
            if len(valores) < 2: return
            
            t1_raw = float(valores[0])
            t2_raw = float(valores[1])
            
            # Ahora procesar devuelve 4 valores
            res = op.procesar(t1_raw, t2_raw)
    
            if res[0] == "llenando":
                graf.mostrar_llenado(res[1])
                return
        
            # Ahora 'res' siempre tendrá (t, diff, t1_f, t2_f) reales
            t, diff, t1_f, t2_f = res
            graf.actualizar(t, diff, t1_f, t2_f)
                    
        except (ValueError, IndexError):
            return

# -------------------------
# QT TIMER
# -------------------------
timer = QtCore.QTimer()
timer.timeout.connect(tick)
timer.start(10)   # 10 ms


if __name__ == "__main__":
    try:
        graf.app.exec()
    finally:
        com.cerrar()
        print("Sistema cerrado limpio.")