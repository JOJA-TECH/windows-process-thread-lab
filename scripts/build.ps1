# build.ps1
# Compila el laboratorio con CMake. Ajusta -Generator y -Platform si es necesario.

param(
    [string]$Generator = "",
    [string]$Platform = "",
    [string]$OutputDir = "build",
    [switch]$Quick
)

$ErrorActionPreference = "Stop"

# Test-Command se define aqui (no se hereda de configure_build.ps1, porque cada
# script se ejecuta en su propio ambito).
function Test-Command($cmd) {
    return $null -ne (Get-Command $cmd -ErrorAction SilentlyContinue)
}

Write-Host "=== Compilacion del laboratorio ===" -ForegroundColor Cyan
Write-Host ""

if (-not (Test-Path $OutputDir)) {
    Write-Host "El directorio de salida '$OutputDir' no existe. Ejecuta primero configure_build.ps1." -ForegroundColor Yellow
    exit 1
}

if (-not (Test-Command cmake)) {
    Write-Host "cmake no se detecta. Instalalo o configura el entorno." -ForegroundColor Red
    exit 1
}

$buildType = if ($Quick) { "Debug" } else { "Release" }
Write-Host "Tipo de build: $buildType" -ForegroundColor Yellow
Write-Host "Generando..." -ForegroundColor Cyan

$cmakeArgs = @("-S", ".", "-B", $OutputDir, "-DCMAKE_BUILD_TYPE=$buildType")
if ($Generator) { $cmakeArgs += @("-G", $Generator) }
if ($Platform) { $cmakeArgs += @("-A", $Platform) }

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host "cmake -S/-B fallo." -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "Compilando..." -ForegroundColor Cyan
if (-not (Test-Command cmake)) {
    Write-Host "No se pudo compilar porque cmake no esta disponible." -ForegroundColor Red
    exit 1
}
& cmake --build $OutputDir --config $buildType
if ($LASTEXITCODE -ne 0) {
    Write-Host "cmake --build fallo." -ForegroundColor Red
    exit $LASTEXITCODE
}

# Buscar el ejecutable
$exeName = if ($env:OS -match "Windows") { "lab.exe" } else { "./lab" }
$exePath = Join-Path $OutputDir $exeName
if (-not (Test-Path $exePath)) {
    # Intentar buscar en arquitectura/Release o Debug
    $candidates = Get-ChildItem -Path $OutputDir -Recurse -File -Filter "lab.exe" | Select-Object -First 1
    if ($candidates) { $exePath = $candidates.FullName }
}
if (Test-Path $exePath) {
    Write-Host ""
    Write-Host "Compilacion exitosa." -ForegroundColor Green
    Write-Host "Ejecutable: $exePath" -ForegroundColor Cyan
} else {
    Write-Host ""
    Write-Host "Compilacion completa pero no se encontro lab.exe automaticamente." -ForegroundColor Yellow
    Write-Host "Busca manualmente en $OutputDir." -ForegroundColor Yellow
}
