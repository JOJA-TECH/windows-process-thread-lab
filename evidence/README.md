# Evidencias del laboratorio

Esta carpeta es para las capturas y notas que debe recolectar el estudiante durante la ejecución. El programa genera automáticamente los archivos de resultados; aquí va la evidencia visual y las observaciones que no pueden generarse automáticamente sin intervención humana.

Cada evidencia debe registrar:

- **ID**
- **Fecha y hora**
- **Experimento**
- **Qué debe verse**
- **Qué significa**
- **Nombre recomendado del archivo**

---

## 1. Información de CPU en PowerShell

- **ID:** E01
- **Fecha y hora:** `YYYY-MM-DD HH:MM:SS`
- **Experimento:** Experimento 1 — caracterización del hardware
- **Qué debe verse:** Salida de `Get-CimInstance Win32_Processor` con nombre, núcleos y procesadores lógicos.
- **Qué significa:** Confirma la configuración real del equipo antes de ejecutar las pruebas.
- **Nombre recomendado:** `evidence/e01_cpu_powershell.png` o `.txt`

## 2. Procesadores lógicos en Administrador de tareas

- **ID:** E02
- **Fecha y hora:** `YYYY-MM-DD HH:MM:SS`
- **Experimento:** Experimento 1
- **Qué debe verse:** Administrador de tareas → Rendimiento → CPU, mostrando la cantidad de procesadores lógicos.
- **Qué significa:** Evidencia visual del valor reportado automáticamente por el programa.
- **Nombre recomendado:** `evidence/e02_logical_processors.png`

## 3. Proceso del laboratorio en Detalles

- **ID:** E03
- **Fecha y hora:** `YYYY-MM-DD HH:MM:SS`
- **Experimento:** Cualquier prueba
- **Qué debe verse:** Administrador de tareas → Detalles, con el proceso `lab.exe` visible.
- **Qué significa:** Confirma que el programa está ejecutándose como proceso observable.
- **Nombre recomendado:** `evidence/e03_lab_process_details.png`

## 4. Utilización de CPU durante prueba secuencial

- **ID:** E04
- **Fecha y hora:** `YYYY-MM-DD HH:MM:SS`
- **Experimento:** Experimento 2 — ejecución secuencial
- **Qué debe verse:** Curva de utilización de CPU mientras corre la prueba secuencial.
- **Qué significa:** Línea base de utilización y tiempo.
- **Nombre recomendado:** `evidence/e04_cpu_sequential.png`

## 5. Utilización de CPU durante prueba con varios hilos

- **ID:** E05
- **Fecha y hora:** `YYYY-MM-DD HH:MM:SS`
- **Experimento:** Experimento 3 — hilos
- **Qué debe verse:** Utilización de CPU durante la ejecución multithread.
- **Qué significa:** Evidencia de concurrencia/uso de múltiples procesadores lógicos.
- **Nombre recomendado:** `evidence/e05_cpu_threads.png`

## 6. Vista por procesador lógico

- **ID:** E06
- **Fecha y hora:** `YYYY-MM-DD HH:MM:SS`
- **Experimento:** Experimento 3 o 5
- **Qué debe verse:** Si está disponible, vista de actividad por procesador lógico en Administrador de tareas o Monitor de recursos.
- **Qué significa:** Ayuda a observar si hay trabajo distribuido entre procesadores lógicos.
- **Nombre recomendado:** `evidence/e06_per_logical_processor.png`

## 7. Prueba de afinidad a un procesador lógico

- **ID:** E07
- **Fecha y hora:** `YYYY-MM-DD HH:MM:SS`
- **Experimento:** Experimento 4 — concurrencia con afinidad
- **Qué debe verse:** Si la prueba se ejecutó, captura que muestre el proceso/hilos confinados a un procesador lógico, o nota de que la afinidad no pudo aplicarse.
- **Qué significa:** Evidencia de que se intentó restringir la ejecución y qué ocurrió.
- **Nombre recomendado:** `evidence/e07_affinity_test.png` o `evidence/e07_affinity_failed.txt`

## 8. Procesos hijos durante el experimento de procesos

- **ID:** E08
- **Fecha y hora:** `YYYY-MM-DD HH:MM:SS`
- **Experimento:** Experimento 6 — procesos
- **Qué debe verse:** Administrador de tareas → Detalles con varios procesos trabajando simultáneamente durante el experimento de procesos.
- **Qué significa:** Evidencia de ejecución real con procesos independientes.
- **Nombre recomendado:** `evidence/e08_processes_worker.png`

## 9. Errores o limitaciones encontradas

- **ID:** E09
- **Fecha y hora:** `YYYY-MM-DD HH:MM:SS`
- **Experimento:** Cualquier experimento
- **Qué debe verse:** Descripción de cualquier error, timeout, descubrimiento de métrica no disponible, o comportamiento inesperado.
- **Qué significa:** Evidencia de limitaciones reales del sistema o del entorno de ejecución.
- **Nombre recomendado:** `evidence/e09_issues.txt`

---

## Notas de registro

- La fecha y hora debe ser la del momento en que se tomó la evidencia, no la del informe final.
- Si alguna evidencia no fue posible, registrala como `no_disponible` y explica por qué.
- Las capturas deben reflejar lo que realmente se observó; no reproduzcas imágenes genéricas ni supongas comportamiento.
