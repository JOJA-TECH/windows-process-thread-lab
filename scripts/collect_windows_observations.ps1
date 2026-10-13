# collect_windows_observations.ps1
# Recolecta datos del sistema para el laboratorio sin requerir privilegios administrativos
# cuando sea posible. El estudiante debe complementar con capturas manuales.

$ErrorActionPreference = "Stop"

Write-Host "=== Recoleccion de observaciones de Windows ===" -ForegroundColor Cyan
Write-Host ""

# CPU basica
Write-Host "Informacion de CPU (Win32_Processor):" -ForegroundColor Yellow
try {
    Get-CimInstance Win32_Processor | Select-Object Name, NumberOfCores, NumberOfLogicalProcessors |
        Format-Table -AutoSize
} catch {
    Write-Host "  No se pudo consultar Win32_Processor: $_" -ForegroundColor Yellow
}
Write-Host ""

# Sistema
Write-Host "Informacion del sistema (Win32_ComputerSystem):" -ForegroundColor Yellow
try {
    Get-CimInstance Win32_ComputerSystem |
        Select-Object NumberOfProcessors, NumberOfLogicalProcessors |
        Format-Table -AutoSize
} catch {
    Write-Host "  No se pudo consultar Win32_ComputerSystem: $_" -ForegroundColor Yellow
}
Write-Host ""

# Contador de uso de CPU
Write-Host "Muestra de uso de CPU (_Total):" -ForegroundColor Yellow
try {
    Get-Counter "\Processor(_Total)\% Processor Time" -SampleInterval 1 -MaxSamples 5 |
        Select-Object -ExpandProperty CounterSamples |
        Format-Table -AutoSize
} catch {
    Write-Host "  No se pudo leer Get-Counter: $_" -ForegroundColor Yellow
}
Write-Host ""

# PID del laboratorio si se esta ejecutando
Write-Host "Proceso del laboratorio (si esta ejecutandose):" -ForegroundColor Yellow
$lab = Get-Process -Name "lab" -ErrorAction SilentlyContinue | Select-Object Id, ProcessName, CPU, WorkingSet64
if ($lab) {
    $lab | Format-Table -AutoSize
} else {
    Write-Host "  El proceso 'lab' no esta ejecutandose actualmente." -ForegroundColor DarkGray
}
Write-Host ""

Write-Host "Evidencias pendientes:" -ForegroundColor Cyan
@"
- Informacion de CPU en PowerShell (captura de la primera consulta).
- Administrador de tareas -> Rendimiento -> CPU: muestra procesadores logicos y uso.
- Administrador de tareas -> Detalles: proceso del laboratorio visible.
- Utilizacion de CPU durante la prueba secuencial.
- Utilizacion de CPU durante la prueba con varios hilos.
- Vista de cada procesador logico si el administrador de tareas lo permite.
- Prueba de afinidad a un procesador logico, si fue exitosa.
- Observacion de procesos hijos durante el experimento de procesos.
- Cualquier error o limitacion observada durante la ejecucion.
"@
