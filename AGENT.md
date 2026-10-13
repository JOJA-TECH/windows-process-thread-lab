# AGENT.md — Laboratorio de Procesos e Hilos en Windows con C++17

## 1. Rol del agente

Actúa como un ingeniero de sistemas operativos y asistente de laboratorio. Debes preparar y automatizar un laboratorio completo sobre **procesos, hilos, concurrencia, paralelismo y planificación de CPU**, exclusivamente en **Windows 10/11**, usando **C++17**.

El resultado debe dejar al estudiante únicamente estas tareas manuales:

1. Compilar y ejecutar los programas en su computador Windows.
2. Revisar y analizar los resultados generados.
3. Redactar el informe final con base en la información recopilada.

No inventes resultados del computador del estudiante. Toda medición que dependa del hardware debe generarse durante la ejecución en Windows y quedar guardada como archivo verificable.

---

## 2. Objetivo del laboratorio

Construir una solución reproducible que permita:

- Identificar el hardware de ejecución.
- Detectar automáticamente los procesadores lógicos disponibles.
- Limitar las pruebas para no ejecutar configuraciones absurdas o excesivas para el equipo.
- Comparar ejecución secuencial, con hilos y con procesos.
- Medir tiempo de pared, tiempo de CPU y utilización aproximada.
- Evidenciar concurrencia y paralelismo.
- Observar el comportamiento del scheduler de Windows.
- Repetir cada configuración y calcular estadísticas.
- Exportar datos en CSV y JSON.
- Generar tablas, gráficas y un borrador técnico de análisis.
- Permitir que el estudiante complete el análisis final con datos reales de su equipo.

---

## 3. Supuestos técnicos obligatorios

1. El sistema operativo objetivo es **Windows 10 u 11 de 64 bits**.
2. El compilador recomendado es **Visual Studio 2022 MSVC** con soporte para C++17.
3. También se puede aceptar MinGW-w64, pero el informe debe indicar el compilador exacto.
4. El lenguaje principal será **C++17**, porque permite:
   - `std::thread` y `std::jthread` si están disponibles.
   - `std::chrono::steady_clock` para mediciones monotónicas.
   - `std::vector`, `std::filesystem` y manejo estructurado de datos.
   - Resultados parciales independientes para evitar condiciones de carrera.
   - Integración con APIs Win32 para procesos, afinidad y métricas.
5. No se debe usar una variable global compartida como acumulador principal de la carga, aunque sea `volatile`, porque introduciría contención o carreras y dañaría la interpretación.
6. Cada hilo debe escribir únicamente en su propia posición de un arreglo de resultados parciales. El resultado final se combina después de hacer `join()`.
7. La carga debe ser determinista: misma cantidad de iteraciones, mismos parámetros y mismo algoritmo en todas las configuraciones.
8. La optimización del compilador debe estar documentada. Para facilitar mediciones iniciales, se puede usar `/O2`, pero la función de trabajo debe producir un resultado observable y validable para evitar que el compilador elimine el cálculo.
9. El programa no debe depender de Internet, permisos administrativos ni software externo para funcionar.
10. Si alguna métrica no puede obtenerse con precisión sin herramientas administrativas, debe registrarse como `NA` y explicarse la limitación; nunca se debe rellenar con un valor inventado.

---

## 4. Regla de detección y límite por hardware

Al iniciar, el programa debe detectar y registrar:

- Nombre de la CPU.
- Número de CPUs físicas habilitadas.
- Núcleos físicos.
- Procesadores lógicos.
- Arquitectura del proceso y del sistema.
- Memoria física total y disponible, si la API lo permite.
- Nombre del sistema operativo.
- Compilador y versión.
- Fecha y hora de la ejecución.

Usa, como mínimo, las siguientes fuentes:

- `GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)` para procesadores lógicos activos.
- `GetActiveProcessorGroupCount()` para grupos de procesadores.
- `GetNativeSystemInfo()` para información de arquitectura.
- `GetLogicalProcessorInformationEx(RelationProcessorCore, ...)` para núcleos físicos y relaciones entre núcleos y procesadores lógicos.
- `GetSystemInfo()` como respaldo.
- Registro de Windows o WMI/PowerShell opcionalmente para el nombre de la CPU, siempre que la dependencia esté disponible.

### 4.1 Cálculo de configuraciones

Define:

```text
LP = cantidad de procesadores lógicos activos
C = cantidad de núcleos físicos detectados
```

Genera una lista de pruebas con valores base:

```text
1, 2, 4, 8, 16, 32
```

Después aplica estas reglas:

- Siempre incluir 1.
- Incluir potencias de 2 menores o iguales que `LP`.
- Incluir `LP` aunque no sea potencia de 2.
- No ejecutar más de `min(LP, 32)` hilos por configuración, salvo que el usuario lo autorice explícitamente.
- Si `LP == 1`, ejecutar únicamente 1 hilo y explicar que no es posible medir paralelismo físico.
- Si `LP < 8`, no ejecutar automáticamente 8 hilos salvo que sea necesario para demostrar sobre-suscripción; en ese caso marcarla como prueba opcional.
- Crear una prueba de sobre-suscripción con `LP + 1` únicamente si `LP < 32`.
- Nunca crear una configuración mayor que 32 hilos en la ejecución automática.
- Para procesos, usar la misma lista de configuraciones, limitada por `min(LP, 16)` para evitar saturación y creación excesiva.
- Si la memoria disponible es baja, reducir el número de procesos y registrarlo como limitación.

El programa debe escribir en `hardware.json` y `hardware.csv` la razón exacta por la que cada configuración fue incluida o excluida.

---

## 5. Estructura obligatoria del proyecto

Divide el código por funcionalidad. No generes un único archivo monolítico.

```text
laboratorio-windows-cpp/
├── AGENT.md
├── README.md
├── CMakeLists.txt
├── include/
│   ├── hardware_info.hpp
│   ├── workload.hpp
│   ├── measurement.hpp
│   ├── thread_experiment.hpp
│   ├── process_experiment.hpp
│   ├── csv_writer.hpp
│   └── json_writer.hpp
├── src/
│   ├── main.cpp
│   ├── hardware_info.cpp
│   ├── workload.cpp
│   ├── measurement.cpp
│   ├── thread_experiment.cpp
│   ├── process_experiment.cpp
│   ├── csv_writer.cpp
│   └── json_writer.cpp
├── scripts/
│   ├── configure_build.ps1
│   ├── build.ps1
│   ├── run_lab.ps1
│   ├── collect_windows_observations.ps1
│   └── make_charts.py
├── results/
│   ├── .gitkeep
│   ├── hardware.json
│   ├── hardware.csv
│   ├── measurements.csv
│   ├── measurements.json
│   ├── observations.txt
│   ├── charts/
│   └── report_draft.md
└── evidence/
    └── README.md
```

Si se usa una estructura distinta, conserva la separación por responsabilidades y actualiza `README.md`.

---

## 6. Diseño de la carga CPU-bound

Implementa una función determinista y reproducible. Requisitos:

- Recibir un rango de iteraciones.
- No modificar memoria compartida durante la ejecución.
- Devolver un resultado entero verificable.
- Usar operaciones suficientemente costosas para que el tiempo sea medible.
- Permitir que cada hilo o proceso reciba un rango distinto.
- Mantener constante el total de iteraciones entre configuraciones.

Ejemplo conceptual:

```cpp
uint64_t cpu_work(uint64_t begin, uint64_t end) {
    uint64_t value = 0x9E3779B97F4A7C15ULL;
    for (uint64_t i = begin; i < end; ++i) {
        value ^= i + 0x9E3779B97F4A7C15ULL + (value << 6) + (value >> 2);
        value = (value * 2862933555777941757ULL) + 3037000493ULL;
    }
    return value;
}
```

El algoritmo puede cambiarse, pero debe permanecer igual en las pruebas secuencial, con hilos y con procesos. Guarda el resultado de verificación para confirmar que todas las variantes realizaron la misma cantidad de trabajo.

Evita:

- `sleep()` como sustituto de trabajo.
- E/S de consola dentro del bucle.
- Acceso concurrente a un acumulador global.
- Asignaciones dinámicas repetidas dentro del bucle.
- Cambiar la carga entre configuraciones.

---

## 7. Experimentos que debe implementar el agente

### Experimento 1: caracterización del hardware

Guardar la información real del sistema en JSON, CSV y texto legible. Mostrar una interpretación automática inicial, sin reemplazar el análisis del estudiante.

### Experimento 2: ejecución secuencial

Ejecutar la carga con un solo flujo de trabajo. Es la línea base.

Registrar:

- Iteraciones totales.
- Resultado de verificación.
- Tiempo de pared.
- Tiempo de CPU del proceso.
- Uso de CPU disponible mediante APIs de Windows, si se puede medir.

### Experimento 3: hilos C++

Usar `std::thread`.

Para cada configuración:

1. Dividir las iteraciones en rangos contiguos.
2. Crear exactamente la cantidad de hilos indicada.
3. Guardar cada resultado en un elemento independiente.
4. Esperar con `join()`.
5. Combinar los resultados.
6. Confirmar que el resultado coincide con la versión secuencial.
7. Medir creación, ejecución y tiempo total por separado cuando sea posible.

### Experimento 4: concurrencia

Ejecutar una prueba con más de un hilo restringida a un solo procesador lógico mediante `SetThreadAffinityMask()` o la API equivalente.

Registrar claramente:

- Que la afinidad fue aplicada o falló.
- La máscara de afinidad utilizada.
- Que varios hilos pueden avanzar de forma intercalada.
- Que no hay paralelismo físico simultáneo si todos están limitados al mismo procesador lógico.

Si aplicar afinidad falla, guardar el código de error de Windows y no afirmar que la afinidad fue aplicada.

### Experimento 5: paralelismo

Ejecutar configuraciones con varios procesadores lógicos disponibles. Observar si el tiempo disminuye y si la utilización total aumenta.

No afirmar que cada hilo permaneció en un núcleo específico: el scheduler puede migrarlo.

### Experimento 6: procesos

Implementar procesos de Windows usando `CreateProcessW()`.

Requisitos:

- Usar un ejecutable trabajador con modo `--worker`.
- El proceso padre debe dividir la carga y pasar al hijo su rango mediante argumentos o un archivo temporal seguro.
- Cada hijo debe escribir su resultado en un archivo independiente.
- El padre debe esperar con `WaitForMultipleObjects()` o una estrategia equivalente.
- Validar los códigos de salida.
- Combinar los resultados después de que todos terminen.
- Evitar que dos procesos escriban simultáneamente en el mismo archivo.
- Limpiar archivos temporales al finalizar, salvo que se solicite conservarlos como evidencia.

Si `CreateProcessW()` no puede ejecutarse, guardar el error Win32 completo y marcar el experimento como fallido, sin simular resultados.

### Experimento 7: repetición estadística

Por configuración ejecutar por defecto:

```text
3 repeticiones válidas
```

Permitir cambiarlo mediante argumento, por ejemplo:

```powershell
lab.exe --repetitions 5
```

Calcular:

- Promedio.
- Mínimo.
- Máximo.
- Rango.
- Desviación estándar, si hay al menos 3 repeticiones.
- Porcentaje de mejora frente a la línea base.
- Speedup.
- Eficiencia aproximada: `speedup / cantidad_de_hilos`.

Una repetición con error de verificación no es válida y debe registrarse como fallida.

---

## 8. Medición y formato de datos

Usa `std::chrono::steady_clock` para tiempo de pared. No uses `system("time ...")`.

Cada fila de `measurements.csv` debe contener como mínimo:

```text
run_id,
experiment,
mode,
workers,
logical_processors,
physical_cores,
iteration_count,
repeat,
wall_time_ms,
process_cpu_time_ms,
worker_cpu_time_ms,
verification_value,
verification_ok,
affinity_requested,
affinity_applied,
peak_memory_bytes,
exit_code,
win32_error_code,
notes
```

El archivo JSON debe contener los mismos datos con tipos correctos, no todos como texto.

Usa codificación UTF-8 y separador de columnas consistente. Escapa correctamente comillas, saltos de línea y caracteres especiales.

---

## 9. Observación de Windows

Genera un script `collect_windows_observations.ps1` que recopile, sin requerir privilegios administrativos cuando sea posible:

```powershell
Get-CimInstance Win32_Processor |
  Select-Object Name, NumberOfCores, NumberOfLogicalProcessors

Get-CimInstance Win32_ComputerSystem |
  Select-Object NumberOfProcessors, NumberOfLogicalProcessors

Get-Counter '\Processor(_Total)\% Processor Time' -SampleInterval 1 -MaxSamples 5
```

Durante la ejecución, el estudiante debe abrir:

- Administrador de tareas → Rendimiento → CPU.
- Administrador de tareas → Detalles.
- Monitor de recursos, si está disponible.

El programa debe imprimir el PID para facilitar su localización.

No afirmes que una captura muestra una observación que no esté visible. El archivo `observations.txt` debe diferenciar:

- Datos medidos automáticamente.
- Datos copiados por el estudiante.
- Observaciones visuales pendientes.

---

## 10. Seguridad y control de carga

Antes de ejecutar:

- Mostrar la cantidad de procesadores lógicos detectados.
- Mostrar la cantidad de iteraciones.
- Mostrar las configuraciones que se ejecutarán.
- Permitir cancelar con `Ctrl+C`.
- No ejecutar automáticamente más de 32 hilos ni más de 16 procesos.
- Establecer un límite de tiempo por prueba, por ejemplo 120 segundos.
- Si se excede, terminar ordenadamente la prueba y registrarla como `timeout`.
- No cambiar prioridad del proceso a tiempo real.
- No modificar permanentemente la afinidad del sistema.
- No apagar, reiniciar ni cambiar configuraciones globales de Windows.

Argumentos recomendados:

```text
--iterations <n>
--repetitions <n>
--max-workers <n>
--max-processes <n>
--timeout-seconds <n>
--skip-processes
--skip-affinity
--output <directory>
--quick
```

El modo `--quick` debe usar menos iteraciones únicamente como prueba funcional y nunca debe presentarse como resultado final del laboratorio.

---

## 11. Validaciones obligatorias

Antes de considerar terminado el laboratorio, el agente debe comprobar:

1. Que el programa compila sin errores.
2. Que el resultado secuencial es válido.
3. Que cada configuración de hilos produce el mismo resultado verificable.
4. Que cada proceso devuelve correctamente su resultado.
5. Que la suma o combinación final coincide con la versión secuencial.
6. Que cada repetición tiene un registro.
7. Que no hay celdas vacías en mediciones obligatorias.
8. Que las configuraciones no exceden los límites del hardware o los límites de seguridad.
9. Que los archivos CSV y JSON se pueden volver a leer.
10. Que las gráficas usan los datos reales de `measurements.csv`.
11. Que cada afirmación del borrador de análisis tiene una referencia a datos o está marcada como hipótesis.

---

## 12. Gráficas y borrador de análisis

Genera, a partir de los CSV:

1. Tiempo promedio vs. número de hilos.
2. Speedup vs. número de hilos.
3. Eficiencia vs. número de hilos.
4. Tiempo de hilos vs. tiempo de procesos.
5. Uso de CPU, si se obtuvieron datos válidos.

El archivo `report_draft.md` debe incluir:

- Objetivo.
- Supuestos.
- Caracterización del equipo.
- Procedimiento.
- Tabla de configuraciones realmente ejecutadas.
- Resultados.
- Gráficas enlazadas.
- Hallazgos automáticos.
- Limitaciones.
- Preguntas del laboratorio con campos para completar.
- Conclusiones preliminares separadas de las conclusiones finales.

Usa esta plantilla de razonamiento para cada resultado:

```text
¿Qué se observa?
¿Qué medición lo demuestra?
¿Por qué podría ocurrir?
¿Qué relación tiene con el número de núcleos y procesadores lógicos?
¿Qué limitación impide una conclusión más fuerte?
```

El agente puede calcular y describir tendencias, pero debe usar expresiones prudentes como “los datos sugieren” cuando no exista una medición directa del scheduler.

---

## 13. Evidencias que debe tomar el estudiante

El agente debe dejar una lista exacta de capturas pendientes en `evidence/README.md`:

- Información de CPU en PowerShell.
- Administrador de tareas mostrando procesadores lógicos.
- Proceso del laboratorio visible en la pestaña Detalles.
- Utilización de CPU durante la prueba secuencial.
- Utilización de CPU durante la prueba con varios hilos.
- Vista de cada procesador lógico, si está disponible.
- Prueba de afinidad a un procesador lógico, si fue exitosa.
- Observación de procesos hijos durante el experimento de procesos.
- Cualquier error o limitación encontrada.

Cada evidencia debe tener:

```text
ID
Fecha y hora
Experimento
Qué debe verse
Qué significa
Nombre recomendado del archivo
```

---

## 14. Límites contra resultados inventados

Está prohibido:

- Inventar cantidad de núcleos o procesadores lógicos.
- Inventar tiempos.
- Crear capturas falsas.
- Presentar resultados del computador del agente como resultados del computador del estudiante.
- Afirmar que hubo paralelismo solo porque se crearon varios hilos.
- Afirmar que un hilo permaneció en un núcleo concreto sin una medición de afinidad válida.
- Rellenar errores con ceros.
- Eliminar configuraciones fallidas sin registrarlas.

Toda ausencia debe aparecer como `NA`, `not_run`, `failed` o `timeout`, con explicación.

---

## 15. Criterio de finalización

El trabajo del agente está completo cuando existen:

- Código C++17 dividido por funcionalidad.
- Proyecto compilable con Visual Studio/CMake.
- Scripts PowerShell de configuración, compilación, ejecución y recolección.
- Detección automática de hardware.
- Límite automático basado en procesadores lógicos.
- Experimento secuencial.
- Experimento con hilos.
- Experimento con procesos, si Windows lo permite.
- Prueba de concurrencia con afinidad, si Windows lo permite.
- Prueba de paralelismo.
- Tres repeticiones por configuración, salvo que el usuario cambie el parámetro.
- CSV y JSON con datos reales.
- Gráficas.
- Lista de capturas pendientes.
- Borrador de informe sin resultados inventados.
- README con instrucciones exactas para ejecutar en Windows.

La salida final debe indicar claramente qué hizo el agente, qué debe ejecutar el estudiante, qué debe analizar y qué debe redactar.