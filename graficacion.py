import pyqtgraph as pg
from pyqtgraph.Qt import QtCore


class GraficadorTiempoReal:
    def __init__(self, ventana=10):
        self.ventana = ventana
        self.tiempos = []
        self.diferencias = []
        self.temperaturas = []

        self.app = pg.mkQApp("Instrumento")
        self.win = pg.GraphicsLayoutWidget(title="Diferencia y Temperatura vs Tiempo")
        self.win.resize(900, 400)
        self.win.show()

        # Plot principal (izquierda)
        self.plot = self.win.addPlot(title="Δ y Temperatura vs Tiempo")
        self.plot.setLabel('left', "Δ Valor")
        self.plot.setLabel('bottom', "Tiempo", units='s')
        self.plot.showGrid(x=True, y=True)

        # Curva de diferencia
        self.curve_diff = self.plot.plot(pen=pg.mkPen(width=2))

        # ViewBox extra para eje derecho (temperatura)
        self.plot_temp = pg.ViewBox()
        self.plot.showAxis('right')
        self.plot.scene().addItem(self.plot_temp)
        self.plot.getAxis('right').linkToView(self.plot_temp)
        self.plot_temp.setXLink(self.plot)
        self.plot.setLabel('right', "Temperatura", units='°C')

        # Curva de temperatura
        self.curve_temp = pg.PlotCurveItem(pen=pg.mkPen(width=2))
        self.plot_temp.addItem(self.curve_temp)

        # Curva de diferencia (rojo)
        self.curve_diff = self.plot.plot(pen=pg.mkPen(color=(255, 0, 0), width=2))

        # Curva de temperatura (azul)
        self.curve_temp = pg.PlotCurveItem(pen=pg.mkPen(color=(0, 120, 255), width=2))
        self.plot_temp.addItem(self.curve_temp)
        
        # Mantener sincronizados los tamaños
        self.plot.vb.sigResized.connect(self._actualizar_vistas)

    def _actualizar_vistas(self):
        self.plot_temp.setGeometry(self.plot.vb.sceneBoundingRect())
        self.plot_temp.linkedViewChanged(self.plot.vb, self.plot_temp.XAxis)

    def actualizar(self, t, diff, temp):
        self.tiempos.append(t)
        self.diferencias.append(diff)
        self.temperaturas.append(temp)

        # Ventana deslizante
        if self.tiempos[-1] > self.ventana:
            t0 = self.tiempos[0]
            self.tiempos[:] = [ti - t0 for ti in self.tiempos[1:]]
            self.diferencias[:] = self.diferencias[1:]
            self.temperaturas[:] = self.temperaturas[1:]

        self.curve_diff.setData(self.tiempos, self.diferencias)
        self.curve_temp.setData(self.tiempos, self.temperaturas)

    def limpiar(self):
        self.tiempos.clear()
        self.diferencias.clear()
        self.temperaturas.clear()
        self.curve_diff.setData([], [])
        self.curve_temp.setData([], [])
