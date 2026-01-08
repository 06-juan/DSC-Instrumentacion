import pyqtgraph as pg

class GraficadorTiempoReal:
    def __init__(self, ventana=10):
        self.ventana = ventana
        self.tiempos = []
        self.diferencias = []

        self.app = pg.mkQApp("Instrumento")
        self.win = pg.GraphicsLayoutWidget(title="Diferencia entre muestras")
        self.win.resize(900, 400)
        self.win.show()

        self.plot = self.win.addPlot(title="Δ Valor vs Tiempo")
        self.plot.setLabel('left', "Δ Valor")
        self.plot.setLabel('bottom', "Tiempo", units='s')
        self.plot.showGrid(x=True, y=True)

        self.curve = self.plot.plot(pen=pg.mkPen(width=2))

    def actualizar(self, t, diff):
        self.tiempos.append(t)
        self.diferencias.append(diff)

        if self.tiempos[-1] > self.ventana:
            self.tiempos[:] = [ti - self.tiempos[0] for ti in self.tiempos]
            self.tiempos[:] = self.tiempos[1:]
            self.diferencias[:] = self.diferencias[1:]

        self.curve.setData(self.tiempos, self.diferencias)

    def limpiar(self):
        self.tiempos.clear()
        self.diferencias.clear()
        self.curve.setData([], [])
