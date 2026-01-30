import pyqtgraph as pg
from pyqtgraph.Qt import QtCore

class GraficadorTiempoReal:
    def __init__(self, ventana=10):
        self.ventana = ventana
        self.tiempos = []
        self.diferencias = []
        self.temperaturas = []
        self.temperaturas2 = []

        # Quitamos pg.mkQApp para que la GUI principal lo maneje
        self.win = pg.GraphicsLayoutWidget(title="Diferencia T1 - T2")
        
        # Plot principal
        self.plot = self.win.addPlot(title="Δ (T1 - T2), T1 y T2 vs Tiempo")
        self.plot.setLabel('left', "Diferencia", units='°C')
        self.plot.setLabel('bottom', "Tiempo", units='s')
        self.plot.showGrid(x=True, y=True)

        # ViewBox extra para eje derecho
        self.plot_temp = pg.ViewBox()
        self.plot.showAxis('right')
        self.plot.scene().addItem(self.plot_temp)
        self.plot.getAxis('right').linkToView(self.plot_temp)
        self.plot_temp.setXLink(self.plot)
        self.plot.setLabel('right', "Temperatura", units='°C')

        # --- CURVAS ---
        self.curve_diff = self.plot.plot(pen=pg.mkPen(color=(255, 255, 255), width=2))
        self.curve_temp = pg.PlotCurveItem(pen=pg.mkPen(color=(255, 150, 0), width=2))
        self.plot_temp.addItem(self.curve_temp)
        self.curve_temp2 = pg.PlotCurveItem(pen=pg.mkPen(color=(0, 120, 255), width=2))
        self.plot_temp.addItem(self.curve_temp2)
        
        self.plot.vb.sigResized.connect(self._actualizar_vistas)

    def _actualizar_vistas(self):
        self.plot_temp.setGeometry(self.plot.vb.sceneBoundingRect())
        self.plot_temp.linkedViewChanged(self.plot.vb, self.plot_temp.XAxis)

    def actualizar(self, t, diff, temp1, temp2):
        self.tiempos.append(t)
        self.diferencias.append(diff)
        self.temperaturas.append(temp1)
        self.temperaturas2.append(temp2)

        if self.tiempos[-1] > self.ventana:
            t0 = self.tiempos[0]
            self.tiempos[:] = [ti - t0 for ti in self.tiempos[1:]]
            self.diferencias[:] = self.diferencias[1:]
            self.temperaturas[:] = self.temperaturas[1:]
            self.temperaturas2[:] = self.temperaturas2[1:]

        self.curve_diff.setData(self.tiempos, self.diferencias)
        self.curve_temp.setData(self.tiempos, self.temperaturas)
        self.curve_temp2.setData(self.tiempos, self.temperaturas2)

    def limpiar(self):
        self.tiempos.clear()
        self.diferencias.clear()
        self.temperaturas.clear()
        self.temperaturas2.clear()
        self.curve_diff.setData([], [])
        self.curve_temp.setData([], [])
        self.curve_temp2.setData([], [])