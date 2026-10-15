# Informe de laboratorio: Procesos e Hilos en Windows

## Objetivo

Comparar el comportamiento de ejecucion secuencial, con hilos y con procesos en Windows para analizar concurrencia, paralelismo y planificacion de CPU, usando una carga CPU-bound deterministica y repeticiones multiples.

## Supuestos

Sistema operativo: Windows 10/11 de 64 bits. Compilador: GCC 10.3.0 (MinGW-w64) (C++201703). La carga usada en secuencial, hilos y procesos es la misma (resultado de verificacion aditivo y comparable). Los resultados deben generarse en el equipo del estudiante; este borrador no inventa mediciones.

## Caracterizacion del equipo

- Nombre de CPU: AMD Ryzen 5 4600G with Radeon Graphics
- Procesadores logicos activos: 12
- Nucleos fisicos detectados: 6
- Sockets/procesadores fisicos: 1
- Grupos de procesadores: 1
- Arquitectura del proceso: x86_64
- Arquitectura del sistema: x86_64
- Sistema operativo: Windows 11 Pro (build 26200, 25H2)
- Compilador/version: GCC 10.3.0 (MinGW-w64) (C++201703)
- Memoria total (bytes): 16963178496
- Memoria disponible (bytes): 6149500928


## Procedimiento

1. Deteccion de hardware automatica y exportacion a hardware.json/hardware.csv.
2. Ejecucion secuencial como linea base.
3. Experimentos con hilos: 6 configuraciones.
4. Prueba de concurrencia con afinidad a un procesador logico.
5. Prueba de paralelismo con varios procesadores logicos.
6. Experimento de procesos (CreateProcessW + archivos de tarea/resultado).
7. Repeticiones por configuracion: 3.
8. Exportacion a measurements.csv/measurements.json, statistics.csv/statistics.json y mediciones.csv/estadisticas.csv (formato comparable con Linux).
9. Graficas generadas a partir de los datos reales (scripts/make_charts.py).
10. Borrador de informe con hallazgos automaticos cautelosos.


## Configuraciones ejecutadas

| experiment | mode | workers | repeat | wall_ms | cpu_ms | worker_cpu_ms | cpu_util_% | verify_ok | affinity | affinity_mask | notes |
|------------|------|---------|--------|---------|--------|---------------|------------|-----------|----------|---------------|-------|
| secuencial | secuencial | 1 | 1 | 707 | 687 | 0 | 97.17 | OK | - | NA | linea base; verificacion_ok=true |
| secuencial | secuencial | 1 | 2 | 712 | 718 | 0 | 100.84 | OK | - | NA | linea base; verificacion_ok=true |
| secuencial | secuencial | 1 | 3 | 705 | 703 | 0 | 99.72 | OK | - | NA | linea base; verificacion_ok=true |
| hilos | 1_hilos | 1 | 1 | 710 | 718 | 0 | 101.13 | OK | - | NA | verificacion_ok |
| hilos | 1_hilos | 1 | 2 | 709 | 703 | 0 | 99.15 | OK | - | NA | verificacion_ok |
| hilos | 1_hilos | 1 | 3 | 712 | 718 | 0 | 100.84 | OK | - | NA | verificacion_ok |
| hilos | 2_hilos | 2 | 1 | 382 | 750 | 0 | 196.34 | OK | - | NA | verificacion_ok |
| hilos | 2_hilos | 2 | 2 | 387 | 765 | 0 | 197.67 | OK | - | NA | verificacion_ok |
| hilos | 2_hilos | 2 | 3 | 541 | 1078 | 0 | 199.26 | OK | - | NA | verificacion_ok |
| hilos | 4_hilos | 4 | 1 | 280 | 1031 | 0 | 368.21 | OK | - | NA | verificacion_ok |
| hilos | 4_hilos | 4 | 2 | 321 | 1140 | 0 | 355.14 | OK | - | NA | verificacion_ok |
| hilos | 4_hilos | 4 | 3 | 270 | 1046 | 0 | 387.41 | OK | - | NA | verificacion_ok |
| hilos | 8_hilos | 8 | 1 | 159 | 1046 | 0 | 657.86 | OK | - | NA | verificacion_ok |
| hilos | 8_hilos | 8 | 2 | 157 | 1125 | 0 | 716.56 | OK | - | NA | verificacion_ok |
| hilos | 8_hilos | 8 | 3 | 170 | 1125 | 0 | 661.76 | OK | - | NA | verificacion_ok |
| hilos | 12_hilos | 12 | 1 | 119 | 1281 | 0 | 1076.47 | OK | - | NA | verificacion_ok |
| hilos | 12_hilos | 12 | 2 | 119 | 1437 | 0 | 1207.56 | OK | - | NA | verificacion_ok |
| hilos | 12_hilos | 12 | 3 | 123 | 1390 | 0 | 1130.08 | OK | - | NA | verificacion_ok |
| hilos | 13_hilos | 13 | 1 | 136 | 1375 | 0 | 1011.03 | OK | - | NA | verificacion_ok |
| hilos | 13_hilos | 13 | 2 | 137 | 1312 | 0 | 957.66 | OK | - | NA | verificacion_ok |
| hilos | 13_hilos | 13 | 3 | 133 | 1390 | 0 | 1045.11 | OK | - | NA | verificacion_ok |
| concurrencia_afinidad | afinidad | 4 | 1 | 861 | 734 | 0 | 85.25 | OK | yes | 1 | afinidad aplicada a LP 0; concurrencia sin paralelismo fisico |
| concurrencia_afinidad | afinidad | 4 | 2 | 746 | 750 | 0 | 100.54 | OK | yes | 1 | afinidad aplicada a LP 0; concurrencia sin paralelismo fisico |
| concurrencia_afinidad | afinidad | 4 | 3 | 778 | 781 | 0 | 100.39 | OK | yes | 1 | afinidad aplicada a LP 0; concurrencia sin paralelismo fisico |
| procesos | 1_procesos | 1 | 1 | 735 | 0 | 718 | 0.00 | OK | - | NA | procesos via archivos de tarea/resultado |
| procesos | 1_procesos | 1 | 2 | 727 | 0 | 718 | 0.00 | OK | - | NA | procesos via archivos de tarea/resultado |
| procesos | 1_procesos | 1 | 3 | 731 | 15 | 703 | 2.05 | OK | - | NA | procesos via archivos de tarea/resultado |
| procesos | 2_procesos | 2 | 1 | 384 | 15 | 718 | 3.91 | OK | - | NA | procesos via archivos de tarea/resultado |
| procesos | 2_procesos | 2 | 2 | 378 | 0 | 718 | 0.00 | OK | - | NA | procesos via archivos de tarea/resultado |
| procesos | 2_procesos | 2 | 3 | 392 | 0 | 734 | 0.00 | OK | - | NA | procesos via archivos de tarea/resultado |
| procesos | 4_procesos | 4 | 1 | 253 | 15 | 810 | 5.93 | OK | - | NA | procesos via archivos de tarea/resultado |
| procesos | 4_procesos | 4 | 2 | 258 | 31 | 842 | 12.02 | OK | - | NA | procesos via archivos de tarea/resultado |
| procesos | 4_procesos | 4 | 3 | 226 | 15 | 763 | 6.64 | OK | - | NA | procesos via archivos de tarea/resultado |
| procesos | 8_procesos | 8 | 1 | 177 | 15 | 982 | 8.47 | OK | - | NA | procesos via archivos de tarea/resultado |
| procesos | 8_procesos | 8 | 2 | 200 | 46 | 1153 | 23.00 | OK | - | NA | procesos via archivos de tarea/resultado |
| procesos | 8_procesos | 8 | 3 | 194 | 0 | 1151 | 0.00 | OK | - | NA | procesos via archivos de tarea/resultado |
| procesos | 12_procesos | 12 | 1 | 230 | 46 | 1198 | 20.00 | OK | - | NA | procesos via archivos de tarea/resultado |
| procesos | 12_procesos | 12 | 2 | 197 | 46 | 1213 | 23.35 | OK | - | NA | procesos via archivos de tarea/resultado |
| procesos | 12_procesos | 12 | 3 | 180 | 46 | 1292 | 25.56 | OK | - | NA | procesos via archivos de tarea/resultado |


## Resultados

Linea base secuencial (media de 3 repeticion(es)): 708.00 ms de tiempo de pared.

Estadisticas por numero de hilos (speedup = linea base / media; eficiencia = speedup / hilos):

| workers | n | media_ms | min_ms | max_ms | rango_ms | sd_ms | speedup | eficiencia | mejora_% |
|---------|---|----------|--------|--------|----------|-------|---------|------------|----------|
| 1 | 3 | 710.33 | 709.00 | 712.00 | 3.00 | 1.53 | 1.00 | 1.00 | -0.33 |
| 2 | 3 | 436.67 | 382.00 | 541.00 | 159.00 | 90.39 | 1.62 | 0.81 | 38.32 |
| 4 | 3 | 290.33 | 270.00 | 321.00 | 51.00 | 27.02 | 2.44 | 0.61 | 58.99 |
| 8 | 3 | 162.00 | 157.00 | 170.00 | 13.00 | 7.00 | 4.37 | 0.55 | 77.12 |
| 12 | 3 | 120.33 | 119.00 | 123.00 | 4.00 | 2.31 | 5.88 | 0.49 | 83.00 |
| 13 | 3 | 135.33 | 133.00 | 137.00 | 4.00 | 2.08 | 5.23 | 0.40 | 80.89 |

Las mediciones completas estan en `results\measurements.csv` y `results\measurements.json`; las estadisticas agregadas se escriben en `statistics.csv` y `statistics.json`; el formato compacto comparable con Linux queda en `mediciones.csv` y `estadisticas.csv`.


## Graficas

Graficas generadas en `results\charts` a partir de measurements.csv:

![Tiempo vs hilos](results\charts/wall_time_by_threads.png)

![Speedup vs hilos](results\charts/speedup_by_threads.png)

![Eficiencia vs hilos](results\charts/efficiency_by_threads.png)

![Hilos vs procesos](results\charts/threads_vs_processes.png)

![Uso de CPU](results\charts/cpu_utilization.png)

Ejecuta `python scripts/make_charts.py --csv results\measurements.csv --out results\charts` si aun no existen.


## Hallazgos automaticos

Hallazgos automaticos (hipotesis a contrastar con datos reales):

- Hilos = 1: media ~710.33 ms (sd 1.53 ms, n=3), speedup ~1.00.
- Hilos = 2: media ~436.67 ms (sd 90.39 ms, n=3), speedup ~1.62.
- Hilos = 4: media ~290.33 ms (sd 27.02 ms, n=3), speedup ~2.44.
- Hilos = 8: media ~162.00 ms (sd 7.00 ms, n=3), speedup ~4.37.
- Hilos = 12: media ~120.33 ms (sd 2.31 ms, n=3), speedup ~5.88.
- Hilos = 13: media ~135.33 ms (sd 2.08 ms, n=3), speedup ~5.23.

Interpretar con cautela: el scheduler de Windows puede migrar hilos entre nucleos; no se afirma que un hilo permanecio en un nucleo concreto sin medicion de afinidad valida.


## Limitaciones

- Nucleos fisicos: 6.
- Tiempos de CPU del proceso pueden incluir overhead de medicion.
- worker_cpu_ms de procesos es la suma del CPU reportado por cada hijo; puede subestimar el total.
- La afinidad solo se marca como aplicada si SetThreadAffinityMask tuvo exito.
- El modo worker usa archivos temporales; problemas de escritura se registran como fallos.
- Los tiempos dependen del equipo del estudiante y no pueden generalizarse.


## Preguntas del laboratorio

1. ¿Que relacion hay entre el numero de nucleos fisicos y el speedup observado?
2. ¿En que configuraciones la eficiencia cae y por que podria ocurrir?
3. ¿Como se comporta el scheduler cuando se crean mas hilos que procesadores logicos?
4. ¿Que diferencias se observan entre hilos y procesos en tiempo y uso de CPU?
5. ¿Que evidencia grafica o de Administrador de tareas respalda las conclusiones?
6. ¿Que limitaciones impiden una conclusion mas firme sobre paralelismo fisico?


## Conclusiones preliminares

- La linea base secuencial tardo 708.00 ms (media).
- Los datos sugieren las tendencias anteriores, pero el analisis final debe basarse en las mediciones y capturas del estudiante.


## Campos para completar por el estudiante

- Fecha y hora de ejecucion local.
- Observaciones de Administrador de tareas.
- Interpretacion propia de cada grafica.
- Respuestas a las preguntas del laboratorio.
- Conclusiones finales.

