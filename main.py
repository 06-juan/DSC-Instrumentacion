from PyQt5 import QtWidgets, QtCore
import time

# Importamos tus clases
from comunicacion import ComunicacionSerial
from operacion import OperadorDiferencia
from graficacion import GraficadorTiempoReal

class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Sistema de Medición Térmica")
        self.resize(1000, 600)

        # 1. ESTADO Y LÓGICA
        self.estado = "idle"
        self.t_inicio = None
        self.com = None # Inicialmente no hay cable conectado
        self.op = OperadorDiferencia()
        
        # 2. COMPONENTES
        self.n = 100 
        self.graficador = GraficadorTiempoReal(ventana=self.n)

        # 3. INTERFAZ (GUI)
        self.init_ui()

        # 4. EL LATIDO DEL SISTEMA (Timer)
        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.tick)
        self.timer.start(10)

    def init_ui(self):
        widget_central = QtWidgets.QWidget()
        self.setCentralWidget(widget_central)
        layout_principal = QtWidgets.QVBoxLayout(widget_central)

        # Fila de controles superior
        controles = QtWidgets.QHBoxLayout()
        
        self.label_n = QtWidgets.QLabel("Tiempo medición (s):")
        self.input_n = QtWidgets.QLineEdit(str(self.n))
        self.input_n.setFixedWidth(80)
        
        self.btn_serial = QtWidgets.QPushButton("Conectar COM5")
        self.btn_serial.clicked.connect(self.toggle_com)
        self.btn_serial.setStyleSheet("height: 30px; font-weight: bold;")

        controles.addWidget(self.label_n)
        controles.addWidget(self.input_n)
        controles.addWidget(self.btn_serial)
        controles.addStretch() # Empuja todo a la izquierda

        # Integramos la gráfica
        layout_principal.addLayout(controles)
        layout_principal.addWidget(self.graficador.win)

    def toggle_com(self):
        """Abre o cierra el puerto serie"""
        if self.com is None:
            try:
                self.com = ComunicacionSerial(puerto="COM5", baudios=115200)
                self.btn_serial.setText("Desconectar COM5")
                self.btn_serial.setStyleSheet("background-color: #ffaaaa; font-weight: bold;")
                print("✅ Puerto Abierto")
            except Exception as e:
                print(f"❌ Error: {e}")
        else:
            self.com.cerrar()
            self.com = None
            self.btn_serial.setText("Conectar COM5")
            self.btn_serial.setStyleSheet("font-weight: bold;")
            print("🔌 Puerto Cerrado")

    def tick(self):
        """Bucle principal de procesamiento"""
        if self.com is None:
            return

        # Leemos el valor de 'n' de la GUI por si cambió
        try:
            self.n = float(self.input_n.text())
            self.graficador.ventana = self.n
        except ValueError:
            pass

        linea = self.com.leer_linea()

        # GESTIÓN DE TIEMPO
        if self.estado == "midiendo" and self.t_inicio:
            if time.time() - self.t_inicio >= self.n:
                self.com.enviar("stop")
                self.estado = "idle"
                self.t_inicio = None
                print("⏰ Tiempo cumplido")

        if linea is None: return
        linea = linea.strip().lower()

        # PROTOCOLO PING (Inicio desde ESP32)
        if linea == "ping" and self.estado == "idle":
            print("\n>> NUEVA MEDICIÓN")
            self.op = OperadorDiferencia()
            self.graficador.limpiar()
            self.com.enviar(f"pong {int(self.n)}")
            self.t_inicio = time.time()
            self.estado = "midiendo"
            return

        # PROCESAMIENTO DE DATOS
        if self.estado == "midiendo":
            try:
                valores = linea.split(",")
                if len(valores) < 4: return
                
                # Desempaquetado (CJC, T1, T2, basura, diff)
                cjc, t1, t2, diff = map(float, valores)
                
                t = self.op.procesar(cjc, t1, t2, diff)
                if t is not None:
                    self.graficador.actualizar(t, diff, t1, t2)
            except (ValueError, IndexError):
                pass

if __name__ == "__main__":
    app = QtWidgets.QApplication([])
    ventana = MainWindow()
    ventana.show()
    app.exec_()