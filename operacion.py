import time

class OperadorDiferencia:
    def __init__(self):
        self.valor_anterior = None
        self.t0 = None

    def procesar(self, valor_actual):
        if self.t0 is None:
            self.t0 = time.time()
            self.valor_anterior = valor_actual
            return None, None  # todavía no hay diferencia

        t = time.time() - self.t0
        diff = valor_actual - self.valor_anterior
        self.valor_anterior = valor_actual

        return t, diff
