# run_lab.ps1
# Ejecuta el laboratorio con argumentos por defecto.
# Ajusta los parametros si quieres otra configuracion.

param(
    [string]$Exe = ".\build\lab.exe",
    [string]$Iterations = "50000000",
    [int]$Repetitions = 3,
    [string]$Output = "results",
    [switch]$Quick,
    [switch]$SkipProcesses,
    [switch]$SkipAffinity,
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Exe)) {
    Write-Host "No se encontro '$Exe'. Compila primero con build.ps1 o ajusta -Exe." -ForegroundColor Red
    exit 1
}

$argsList = @()
$argsList += "--output", $Output
$argsList += "--iterations", $Iterations
$argsList += "--repetitions", [string]$Repetitions
$argsList += "--timeout-seconds", [string]$TimeoutSeconds
if ($Quick) { $argsList += "--quick" }
if ($SkipProcesses) { $argsList += "--skip-processes" }
if ($SkipAffinity) { $argsList += "--skip-affinity" }

Write-Host "=== Ejecucion del laboratorio ===" -ForegroundColor Cyan
Write-Host "Ejecutable: $Exe" -ForegroundColor Yellow
Write-Host "Args: $($argsList -join ' ')" -ForegroundColor Yellow
Write-Host ""

$proc = Start-Process -FilePath $Exe -ArgumentList $argsList -NoNewWindow -Wait -PassThru
if ($proc.ExitCode -ne 0) {
    Write-Host "El laboratorio finalizo con codigo de salida $($proc.ExitCode)." -ForegroundColor Yellow
} else {
    Write-Host "El laboratorio finalizo correctamente." -ForegroundColor Green
}

Write-Host ""
Write-Host "Archivos esperados en results/:" -ForegroundColor Cyan
@(
    "hardware.json",
    "hardware.csv",
    "measurements.csv",
    "measurements.json",
    "observations.txt",
    "report_draft.md"
) | ForEach-Object {
    $p = Join-Path $Output $_
    if (Test-Path $p) {
        Write-Host "  [OK] $_" -ForegroundColor Green
    } else {
        Write-Host "  [--] $_ (no generado)" -ForegroundColor DarkGray
    }
}

$charts = Join-Path $Output "charts"
if (Test-Path $charts) {
    Write-Host ""
    Write-Host "Graficas:" -ForegroundColor Cyan
    Get-ChildItem -Path $charts -File | ForEach-Object {
        Write-Host "  [OK] charts/$($_.Name)" -ForegroundColor Green
    }
}

# Generar graficas si hay Python + matplotlib disponibles (no es obligatorio).
$chartScript = Join-Path $PSScriptRoot "make_charts.py"
$csvPath = Join-Path $Output "measurements.csv"
if ((Test-Path $chartScript) -and (Test-Path $csvPath)) {
    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($python) {
        Write-Host ""
        Write-Host "Generando graficas con make_charts.py..." -ForegroundColor Cyan
        & python $chartScript --csv $csvPath --out $charts
        if ($LASTEXITCODE -ne 0) {
            Write-Host "No se pudieron generar las graficas (revisa matplotlib). Puedes generarlas manualmente." -ForegroundColor Yellow
        }
    } else {
        Write-Host "Python no detectado: genera las graficas manualmente con scripts/make_charts.py." -ForegroundColor DarkGray
    }
}
