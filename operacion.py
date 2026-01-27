import time
import csv
from collections import deque

class OperadorDiferencia:
    def __init__(self, archivo="datos_serial.csv"):
        self.t0 = None
        self.nombre_archivo = archivo
        self.filtro_listo = False  # Nueva bandera
        
        # ==========================================================
        # CONFIGURACIÓN DEL FILTRO FIR
        # ==========================================================
        # PEGA AQUÍ TUS COEFICIENTES
        self.coeffs = [
    -0.000167712111828966,
    0.000560741896165181,
    0.001511360635902223,
    0.002642615138536671,
    0.003566471507598460,
    0.003550006673007395,
    0.001753386343065659,
    -0.002339570917836489,
    -0.008446529524105878,
    -0.015173421415750274,
    -0.020048204740477173,
    -0.019985457774202443,
    -0.012113361536702682,
    0.005255093723605808,
    0.031814154660597144,
    0.064916838550701211,
    0.099864541219593095,
    0.130822182841141998,
    0.152143559517899818,
    0.159746610626178315,
    0.152143559517899818,
    0.130822182841141998,
    0.099864541219593109,
    0.064916838550701225,
    0.031814154660597144,
    0.005255093723605808,
    -0.012113361536702680,
    -0.019985457774202443,
    -0.020048204740477176,
    -0.015173421415750274,
    -0.008446529524105881,
    -0.002339570917836491,
    0.001753386343065660,
    0.003550006673007394,
    0.003566471507598456,
    0.002642615138536674,
    0.001511360635902223,
    0.000560741896165181,
    -0.000167712111828966,
]
        # ==========================================================
        
        self.num_taps = len(self.coeffs)
        self.buffer_t1 = deque(maxlen=self.num_taps)
        self.buffer_t2 = deque(maxlen=self.num_taps)
        
        self.f = None
        self.writer = None
        # Ya no abrimos el archivo aquí, lo haremos cuando el filtro esté listo
        self.warmup_counts = 20 
        self.current_warmup = 0

    def filtrar(self, buffer):
        """Calcula el valor filtrado usando los coeficientes"""
        resultado = 0.0
        for i in range(self.num_taps):
            resultado += buffer[i] * self.coeffs[i]
        return resultado

    def abrir_archivo(self):
        """Crea el archivo CSV con la cabecera"""
        self.f = open(self.nombre_archivo, "w", newline="")
        self.writer = csv.writer(self.f)
        self.writer.writerow(["tiempo_s", "t1_raw", "t2_raw", "t1_filt", "t2_filt", "diff_filt"])
        self.f.flush()

    def procesar(self, t1, t2):
        # 1. Descarte eléctrico (Warm-up)
        if self.current_warmup < self.warmup_counts:
            self.current_warmup += 1
            return "llenando", 0, None, None

        # 2. Verificar si el filtro ya tiene suficientes datos
        self.buffer_t1.append(t1)
        self.buffer_t2.append(t2)

        if not self.filtro_listo:
            porcentaje = int((len(self.buffer_t1) / self.num_taps) * 100)
            if len(self.buffer_t1) == self.num_taps:
                self.filtro_listo = True
                self.t0 = time.time()
                self.abrir_archivo()
            else:
                return "llenando", porcentaje, None, None

        # 3. Si llegamos aquí, el filtro está listo
        t = time.time() - self.t0
        
        t1_f = self.filtrar(self.buffer_t1)
        t2_f = self.filtrar(self.buffer_t2)
        diff_f = t1_f - t2_f

        # 4. Guardar solo datos válidos
        if self.writer:
            self.writer.writerow([
                f"{t:.3f}", f"{t1:.3f}", f"{t2:.3f}", 
                f"{t1_f:.3f}", f"{t2_f:.3f}", f"{diff_f:.3f}"
            ])
            self.f.flush()

        return t, diff_f, t1_f, t2_f

    def cerrar(self):
        if self.f:
            self.f.close()
            self.f = None
            self.writer = None