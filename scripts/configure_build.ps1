# configure_build.ps1
# Prepara el entorno para compilar el laboratorio en Windows.
# No pide privilegios administrativos.

param(
    [string]$Generator = "",
    [string]$Platform = "",
    [string]$OutputDir = "build"
)

$ErrorActionPreference = "Stop"

Write-Host "=== Laboratorio de Procesos e Hilos - Configuracion de compilacion ===" -ForegroundColor Cyan
Write-Host ""

# Detectar herramientas basicas
function Test-Command($cmd) {
    return $null -ne (Get-Command $cmd -ErrorAction SilentlyContinue)
}

$tools = @{
    cmake = Test-Command cmake
    "c++" = Test-Command "g++" -ErrorAction SilentlyContinue
    cl = Test-Command cl -ErrorAction SilentlyContinue
    msbuild = Test-Command msbuild -ErrorAction SilentlyContinue
}

Write-Host "Herramientas detectadas:" -ForegroundColor Yellow
foreach ($kvp in $tools.GetEnumerator()) {
    $name = $kvp.Key
    $present = $kvp.Value
    if ($present) {
        Write-Host "  [OK] $name" -ForegroundColor Green
    } else {
        Write-Host "  [--] $name (no detectado)" -ForegroundColor DarkGray
    }
}
Write-Host ""

if (-not $tools.cmake) {
    Write-Host "CMake no se detecto. Instalalo desde https://cmake.org/download/ o configura un entorno con MSVC / MinGW-w64." -ForegroundColor Red
    exit 1
}

$cmakeVersion = cmake --version | Select-String -Pattern "cmake version" | ForEach-Object { $_.Line.Trim() }
Write-Host "CMake: $cmakeVersion" -ForegroundColor Cyan
Write-Host ""

# Elegir generator automaticamente si no se proporciona
if (-not $Generator) {
    if ($tools.cl -or $tools.msbuild) {
        $vcVars = $env:VCINSTALLDIR
        if ($vcVars) {
            $Generator = "Visual Studio 17 2022"
            Write-Host "Generador recomendado: Visual Studio 17 2022 (detectado MSVC)" -ForegroundColor Green
        } else {
            # Si no hay VCINSTALLDIR pero existe cl, avanzamos con un intento
            $Generator = "Visual Studio 17 2022"
            Write-Host "Generador elegido: Visual Studio 17 2022 (presencia de cl)" -ForegroundColor Yellow
        }
    } elseif ($tools["c++"]) {
        $Generator = "MinGW Makefiles"
        Write-Host "Generador recomendado: MinGW Makefiles (detectado g++)" -ForegroundColor Green
    } else {
        Write-Host "No se detecto un compilador conocido. Especifica -Generator <nombre>." -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "Generador solicitado: $Generator" -ForegroundColor Cyan
}

# Preparar directorios
Write-Host ""
Write-Host "Preparando directorios..." -ForegroundColor Yellow
@($OutputDir, "$OutputDir\results", "$OutputDir\results\charts", "$OutputDir\results\tmp", "evidence") |
    ForEach-Object {
        if (-not (Test-Path $_)) {
            New-Item -Path $_ -ItemType Directory | Out-Null
            Write-Host "  Creado: $_" -ForegroundColor DarkGray
        }
    }

# Resumen de comando sugerido
Write-Host ""
Write-Host "Comando de generacion sugerido:" -ForegroundColor Cyan
$suggested = "cmake -S . -B $OutputDir -G `"$Generator`""
if ($Platform) {
    $suggested += " -A $Platform"
}
Write-Host "  $suggested" -ForegroundColor White
Write-Host ""
Write-Host "A continuacion, ejecuta build.ps1 para compilar." -ForegroundColor Cyan
