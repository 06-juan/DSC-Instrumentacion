import matplotlib.pyplot as plt

class GraficadorTiempoReal:
    def __init__(self, ventana=100):
        self.VENTANA = ventana
        self.tiempos = []
        self.diferencias = []

        self.fig, self.ax = plt.subplots()
        self.line, = self.ax.plot([], [], lw=2)

        self.ax.set_xlabel("Tiempo (s)")
        self.ax.set_ylabel("Δ Valor")
        self.ax.set_title("Diferencia entre muestras – ventana 100 s")
        self.ax.grid(True)
        self.ax.set_xlim(0, self.VENTANA)
        self.ax.set_ylim(-0.5, 0.5)

    def actualizar(self, t, diff):
        self.tiempos.append(t)
        self.diferencias.append(diff)

        # mantener ventana fija
        if self.tiempos[-1] > self.VENTANA:
            self.tiempos[:] = [ti - self.tiempos[0] for ti in self.tiempos]
            self.tiempos[:] = self.tiempos[1:]
            self.diferencias[:] = self.diferencias[1:]

        self.line.set_data(self.tiempos, self.diferencias)
        plt.pause(0.001)

    def mostrar(self):
        plt.show()
