# windows-process-thread-lab

Laboratorio experimental en C++17 para Windows que analiza procesos, hilos, concurrencia, paralelismo y planificación de CPU.

Detecta automáticamente el hardware, limita las pruebas según los procesadores lógicos, compara ejecución secuencial, con hilos y con procesos, mide tiempos, calcula estadísticas y genera datos, gráficas y un borrador de informe.

El resultado deja al estudiante únicamente estas tareas manuales:

1. Compilar y ejecutar los programas en su equipo Windows.
2. Revisar y analizar los resultados generados.
3. Redactar el informe final con base en la información recopilada.

No se inventan resultados del computador del estudiante. Toda medición que dependa del hardware se genera durante la ejecución en Windows y queda guardada como archivo verificable.

## Documentación para revisión

- **[Arquitectura y guía de revisión](docs/ARCHITECTURE.md):** flujo completo, módulos C++,
  experimentos, carga, métricas, contratos de salida, scripts, evidencias, limitaciones y
  checklist para el evaluador.
- **[Reglas técnicas del proyecto](AGENT.md):** requisitos y criterios de implementación.
- **[Checklist de evidencias](evidence/README.md):** capturas y observaciones manuales E01–E09.
- **[Descripción de archivos generados](results/README.md):** contenido y ciclo de vida de cada salida.

## Requisitos

- Windows 10 u 11 de 64 bits.
- Compilador compatible con C++17: Visual Studio 2022 con MSVC, o MinGW-w64.
- CMake 3.16 o mayor.
- Python 3.8+ y `matplotlib` si se desea regenerar las gráficas con `scripts/make_charts.py` (opcional).

## Estructura resumida

```text
windows-process-thread-lab/
├── CMakeLists.txt
├── AGENT.md
├── README.md
├── LICENSE
├── include/
│   ├── terminal.hpp            # salida por consola (color, secciones, progreso)
│   ├── json.hpp                # JSON mínimo (escritura + lectura de tareas del worker)
│   ├── csv_writer.hpp          # escritor CSV con escape correcto
│   ├── args.hpp                # opciones de línea de comandos
│   ├── hardware_info.hpp       # detección de hardware y plan de configuraciones
│   ├── workload.hpp            # carga CPU-bound determinista y reparto de iteraciones
│   ├── measurement.hpp         # tiempo de pared / CPU (steady_clock + Win32)
│   ├── experiment_context.hpp  # contexto y fila de medición compartidos
│   ├── thread_experiment.hpp   # secuencial, hilos y afinidad
│   ├── process_experiment.hpp  # experimento de procesos (CreateProcessW)
│   └── report_draft.hpp        # bloques y render del borrador de informe
├── src/
│   ├── main.cpp
│   ├── terminal.cpp
│   ├── json.cpp
│   ├── csv_writer.cpp
│   ├── args.cpp
│   ├── hardware_info.cpp
│   ├── workload.cpp
│   ├── measurement.cpp
│   ├── thread_experiment.cpp
│   ├── process_experiment.cpp
│   └── report_draft.cpp
├── scripts/
│   ├── configure_build.ps1
│   ├── build.ps1
│   ├── run_lab.ps1
│   ├── collect_windows_observations.ps1
│   └── make_charts.py
├── results/                    # se llena al ejecutar (incluye .gitkeep y README.md)
└── evidence
    └── README.md
```

La explicación funcional de cada archivo y la arquitectura de ejecución están en
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md); esta sección se mantiene como mapa rápido.

## Compilación en Windows

Con Visual Studio 2022 (CMake, MSVC):

```powershell
.\scripts\configure_build.ps1
.\scripts\build.ps1
```

Con MinGW-w64:

```powershell
.\scripts\configure_build.ps1 -Generator "MinGW Makefiles"
.\scripts\build.ps1
```

Si el generador no se detecta automáticamente, especifícalo con `-Generator`.

Hay que compilar en Windows; el programa usa APIs Win32 (kernel32 y psapi) y no está pensado para otras plataformas.

## Ejecución

```powershell
.\scripts\run_lab.ps1
```

El script abre automáticamente el Administrador de tareas antes de iniciar
`lab.exe`, para facilitar las capturas de Rendimiento → CPU y Detalles durante
las pruebas. Si no se desea abrirlo, usa:

```powershell
.\scripts\run_lab.ps1 -NoTaskManager
```

O directamente:

```powershell
.\build\lab.exe --output results --iterations 50000000 --repetitions 3
```

Modo rápido, solo para verificar que funciona (no es válido como resultado final):

```powershell
.\build\lab.exe --quick
```

### Argumentos importantes

- `--output <dir>`: directorio de resultados (`results` por defecto).
- `--iterations <n>`: cantidad total de iteraciones de carga por ejecución.
- `--repetitions <n>`: repeticiones por configuración (3 por defecto).
- `--max-workers <n>`: límite manual para hilos.
- `--max-processes <n>`: límite manual para procesos.
- `--timeout-seconds <n>`: tiempo máximo por prueba.
- `--skip-processes`: omite el experimento de procesos.
- `--skip-affinity`: omite la prueba de afinidad.
- `--save-evidence`: conserva los archivos temporales del worker.
- `--quick`: modo funcional rápido, no es un resultado de laboratorio.
- `--no-color`: desactiva colores ANSI.
- `--quiet`: salida mínima.
- `--worker --task <file> [--result <file>]`: modo trabajador (uso interno del experimento de procesos).

### Modo worker

El mismo ejecutable actúa como trabajador en el experimento de procesos:

```powershell
.\build\lab.exe --worker --task results/tmp/task_1.json --result results/tmp/result_1.json
```

El padre crea un archivo de tarea JSON por hijo, lanza todos los hijos, espera con `WaitForMultipleObjects`, valida los códigos de salida y combina los resultados. Así la carga es idéntica entre secuencial, hilos y procesos.

## Qué hace el programa

1. Detecta hardware (CPU, núcleos/sockets, procesadores lógicos, arquitectura, memoria, SO, compilador) y lo guarda en `hardware.json` y `hardware.csv`, con la justificación de cada configuración incluida o excluida.
2. Ejecuta la carga secuencial como línea base.
3. Ejecuta configuraciones con hilos, manteniendo constante el trabajo total.
4. Ejecuta una prueba de concurrencia con afinidad a un procesador lógico.
5. Ejecuta pruebas de paralelismo con varios procesadores lógicos.
6. Ejecuta procesos independientes con `CreateProcessW()` y archivos de tarea/resultado.
7. Repite cada configuración, incluida la línea base secuencial, y calcula estadísticas agregadas (media, mínimo, máximo, rango, desviación estándar, mejora %, speedup y eficiencia) en `statistics.csv` y `statistics.json`.
8. Genera `mediciones.csv` y `estadisticas.csv` con el formato compacto comparable con el repositorio Linux.
9. Genera un borrador de informe en `report_draft.md` con la tabla de configuraciones y las estadísticas.
10. Escribe observaciones en `observations.txt` separando datos medidos, copiados por el estudiante y pendientes.

### Verificación de la carga

`cpu_work(begin, end)` usa SplitMix64 y acumula en módulo 2^63. El checksum es **aditivo** respecto a la partición del rango: la suma de los parciales de los trabajadores es igual al valor de la línea base secuencial. Por eso `verification_ok` es significativo y comparable entre secuencial, hilos y procesos.

## Resultados

Los archivos se guardan en el directorio de salida (por defecto `results/`):

- `hardware.json`, `hardware.csv` — caracterización del equipo y razones de configuración.
- `measurements.csv`, `measurements.json` — una fila por ejecución con todos los campos requeridos.
- `statistics.csv`, `statistics.json` — estadísticas agregadas por experimento y configuración, calculadas solo con ejecuciones verificadas.
- `mediciones.csv`, `estadisticas.csv` — salidas compactas compatibles con el repositorio Linux, con los campos `rep`, `mode`, `units`, `logical_cpus`, `total_iters`, `wall_s`, `cpu_s`, `util_pct` y `checksum`.
- `observations.txt` — observaciones medidas/copiadas/pendientes.
- `report_draft.md` — borrador de informe (incluye enlaces a las gráficas).
- `charts/*.png` — gráficas generadas por `scripts/make_charts.py`.

Genera o regenera las gráficas:

```powershell
python scripts/make_charts.py --csv results/measurements.csv --out results/charts
```

`make_charts.py` detecta la línea base secuencial desde el propio CSV (`--baseline-ms` permite sobrescribirla) y produce: tiempo vs. hilos, speedup, eficiencia, hilos vs. procesos y uso de CPU.

## Estado de las métricas

Si una métrica no puede obtenerse sin herramientas administrativas o no está disponible, se registra como `NA` y se explica la limitación. No se rellena con valores inventados.

- `worker_cpu_time_ms` de procesos es la suma del CPU reportado por cada hijo (puede subestimar el total).
- En `mediciones.csv`, `cpu_s` usa el CPU del proceso para secuencial/hilos y la suma `worker_cpu_time_ms` para procesos, equivalente a `RUSAGE_CHILDREN` del experimento Linux.
- La afinidad solo se marca como aplicada si `SetThreadAffinityMask` tuvo éxito; si falla se guarda el código de error.
- `cpu_utilization_percent` es CPU del proceso / tiempo de pared; puede superar 100% cuando varios hilos/núcleos trabajan a la vez.
- `affinity_mask` registra la máscara solicitada en la prueba de afinidad; la implementación actual usa la máscara `1` para el procesador lógico 0.
- La prueba de afinidad usa 4 hilos en el procesador lógico 0, para mantener la misma configuración experimental documentada en Linux.

## Evidencias

El estudiante debe recolectar evidencias adicionales según `evidence/README.md`, incluyendo capturas del Administrador de tareas, información de CPU en PowerShell y observaciones durante las pruebas.

## Limitaciones conocidas

- El nombre de CPU y la versión del SO se leen del registro de Windows cuando es posible.
- Los núcleos/sockets se detectan con `GetLogicalProcessorInformationEx`; si no se puede, se registra como `NA`.
- El scheduler de Windows puede mover hilos entre núcleos; no se afirma que un hilo permaneció en un núcleo concreto sin medición de afinidad válida.
- El límite de tiempo por prueba se aplica a los procesos; en las pruebas con hilos se registra como `timeout_excedido` en las notas.

## Reconocimiento

Este laboratorio está diseñado para fines académicos de análisis de procesos, hilos, concurrencia, paralelismo y planificación de CPU en Windows.
