# results/ — salida del laboratorio

Esta carpeta es **generada por el programa** `lab.exe` durante la ejecución en el
equipo del estudiante. Los archivos que ves versionados aquí son **plantillas
vacías** (sin datos reales): el programa los sobrescribe por completo cada vez que
se ejecuta.

> Regla del laboratorio (AGENT.md §14): no se inventan tiempos, núcleos ni
> procesadores lógicos. Los valores reales solo aparecen tras ejecutar el
> laboratorio en tu propia máquina.

## Qué se genera y cuándo

| Archivo | Lo escribe | Contenido |
|---|---|---|
| `hardware.json` | `lab.exe` (Experimento 1) | Hardware real detectado con las APIs de Windows + razón de cada configuración. |
| `hardware.csv` | `lab.exe` (Experimento 1) | El mismo hardware en formato vertical `field,value,notes`. |
| `measurements.csv` | `lab.exe` (al final) | Una fila por ejecución: tiempos, verificación, afinidad, errores, notas. |
| `measurements.json` | `lab.exe` (al final) | Los mismos datos con tipos correctos (`int`, `double`, `bool`), no todo texto. |
| `observations.txt` | `lab.exe` (al final) | Separado en: datos medidos / datos que copia el estudiante / pendientes. |
| `report_draft.md` | `lab.exe` (al final) | Borrador del informe con tablas, enlaces a las gráficas y hallazgos cautelosos. |
| `charts/*.png` | `scripts/make_charts.py` | Gráficas construidas a partir de `measurements.csv`. |
| `tmp/` | `lab.exe` (Experimento 6) | Archivos de tarea/resultado de los procesos hijos; se limpian al terminar salvo `--save-evidence`. |

## Cómo generarlos

```powershell
# 1) Compilar
.\scripts\build.ps1

# 2) Ejecutar (genera hardware.*, measurements.*, observations.txt, report_draft.md)
.\scripts\run_lab.ps1

# 3) (Opcional) Regenerar las gráficas a partir del CSV real
python .\scripts\make_charts.py --csv .\results\measurements.csv --out .\results\charts
```

`--quick` produce muy pocas iteraciones y **no** sirve como resultado final del
informe; úsalo solo para comprobar que todo funciona.

## Estado de las plantillas versionadas

- `hardware.json` / `hardware.csv` / `measurements.json` incluyen el campo
  `_placeholder` (o valores `0`/`NA` con nota) para dejar claro que **no** hay
  datos reales.
- `measurements.csv` solo contiene la fila de encabezados.
- `observations.txt` y `report_draft.md` están redactados como plantilla.
