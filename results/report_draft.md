# Informe de laboratorio: Procesos e Hilos en Windows

> Plantilla sin datos reales. `lab.exe` sobrescribe este archivo al ejecutarse y
> rellena los campos `<...>` con los datos del equipo del estudiante.

## Objetivo

Comparar el comportamiento de ejecución secuencial, con hilos y con procesos en Windows para analizar concurrencia, paralelismo y planificación de CPU, usando una carga CPU-bound determinista y repeticiones múltiples.

## Supuestos

Sistema operativo: Windows 10/11 de 64 bits. Compilador: `<compiler_id>`. La carga usada en secuencial, hilos y procesos es la misma. Los resultados del estudiante deben generarse en su propia máquina; este borrador no inventa mediciones.

## Caracterización del equipo

- Nombre de CPU: `<cpu_name>`
- Procesadores lógicos activos: `<logical_processors>`
- Núcleos físicos detectados: `<physical_cores>`
- Procesadores físicos habilitados: `<physical_cpus>`
- Arquitectura: `<architecture>`
- Sistema operativo: `<os_name>`
- Compilador/version: `<compiler_id>`
- Memoria total (bytes): `<total_memory_bytes>`
- Memoria disponible (bytes): `<available_memory_bytes>`

Rationale de configuración: `<configuration_rationale>`

Limitaciones detectadas: `<limitations>`

## Procedimiento

1. Detección de hardware automática y exportación a hardware.json/hardware.csv.
2. Ejecución secuencial como línea base, repetida el mismo número de veces que las demás configuraciones.
3. Experimentos con hilos: configuraciones elegidas automáticamente.
4. Prueba de concurrencia con afinidad si el sistema lo permite.
5. Prueba de paralelismo con varios procesadores lógicos.
6. Experimento de procesos si no se saltó y Windows lo permite.
7. Repeticiones por configuración: `<repetitions>`.
8. Exportación a measurements.csv y measurements.json.
9. Gráficas generadas a partir de los datos reales.
10. Borrador de informe con hallazgos automáticos cautelosos.

## Configuraciones ejecutadas

| experiment | mode | workers | repeat | wall_ms | cpu_ms | worker_cpu_ms | verify_ok | affinity_ok | affinity_mask | notes |
|------------|------|---------|--------|---------|--------|---------------|-----------|-------------|---------------|-------|

## Resultados

Los datos sugieren que la línea base secuencial tarda `<baseline_wall_ms>` ms de tiempo de pared.

Las mediciones se guardaron en `measurements.csv` y `measurements.json`; las estadísticas agregadas se guardaron en `statistics.csv` y `statistics.json`. Para comparar directamente con Linux también se generan `mediciones.csv` y `estadisticas.csv`.

Cada afirmación de speedup o eficiencia debe verificarse con los datos reales del equipo.

## Gráficas

Ver directorio `results/charts` para las gráficas generadas a partir de measurements.csv.

## Hallazgos automáticos

Hallazgos automáticos (hipótesis a contrastar con datos reales):

- Los tiempos por número de hilos se analizan a partir de los datos reales.
- Las observaciones deben interpretarse con cautela: el scheduler de Windows puede migrar hilos entre núcleos, y no se puede afirmar que un hilo permaneció en un núcleo concreto sin medición de afinidad válida.

## Limitaciones

- Núcleos físicos: `<physical_cores>` (NA si no se detectó).
- Tiempos de CPU del proceso pueden incluir overhead de medición.
- Afinidad aplicada sin confirmación directa puede no reflejar el comportamiento real del scheduler.
- El modo worker usa archivos temporales; si hay problemas de escritura, se registran como fallos.
- Los tiempos dependen del equipo del estudiante y no pueden generalizarse.

## Preguntas del laboratorio

Completar con los datos obtenidos:

1. ¿Qué relación hay entre el número de núcleos físicos y el speedup observado?
2. ¿En qué configuraciones la eficiencia cae y por qué podría ocurrir?
3. ¿Cómo se comporta el scheduler cuando se crean más hilos que procesadores lógicos?
4. ¿Qué diferencias se observan entre hilos y procesos en tiempo de respuesta y uso de CPU?
5. ¿Qué evidencia gráfica o de Administrador de tareas respalda las conclusiones?
6. ¿Qué limitaciones impiden una conclusión más firme sobre paralelismo físico?

## Conclusiones preliminares

Conclusiones preliminares (basadas únicamente en los datos generados automáticamente):

- La línea base secuencial tardó `<baseline_wall_ms>` ms.
- Se observaron variaciones entre configuraciones; sin datos del equipo del estudiante no se puede concluir nada firme.
- El estudiante debe completar el análisis con sus mediciones y evidencias.

## Campos para completar por el estudiante

Completar con los datos del estudiante:

- Fecha y hora de ejecución local.
- Observaciones de Administrador de tareas.
- Interpretación propia de cada gráfica.
- Respuestas a las preguntas del laboratorio.
- Conclusiones finales.
