cmake_minimum_required(VERSION 3.16)

# El laboratorio es exclusivamente para Windows (usa APIs Win32). Se declara CXX
# como unico lenguaje; no se fuerza arquitectura para respetar al generador.
project(laboratorio_wx LANGUAGES CXX)

# ----------------------------------------------------------------------------
# Opciones del proyecto
# ----------------------------------------------------------------------------
option(LAB_BUILD_WORKER_ONLY "Compilar solo los puntos de entrada del worker (depuracion)" OFF)

# ----------------------------------------------------------------------------
# Lenguaje
# ----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  # /O2 se usa en Release. La funcion de trabajo produce un resultado
  # observable y verificable, por lo que el compilador no elimina el calculo.
  add_compile_options(/W4 /permissive- /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ----------------------------------------------------------------------------
# Archivos del proyecto
# ----------------------------------------------------------------------------
set(LAB_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(LAB_SOURCES
  src/terminal.cpp
  src/json.cpp
  src/csv_writer.cpp
  src/args.cpp
  src/hardware_info.cpp
  src/workload.cpp
  src/measurement.cpp
  src/thread_experiment.cpp
  src/process_experiment.cpp
  src/report_draft.cpp
  src/main.cpp
)

add_executable(lab ${LAB_SOURCES})
target_include_directories(lab PRIVATE ${LAB_INCLUDE_DIRS})

# ----------------------------------------------------------------------------
# Configuracion especifica de Windows
# ----------------------------------------------------------------------------
if(WIN32)
  # GetLogicalProcessorInformationEx requiere Windows 7 o superior.
  target_compile_definitions(lab PRIVATE
    _UNICODE UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS
    _WIN32_WINNT=0x0601)
  # GetProcessMemoryInfo vive en Psapi; el resto son APIs de kernel32/user32.
  target_link_libraries(lab PRIVATE psapi)
else()
  message(FATAL_ERROR "Este laboratorio usa APIs de Windows y solo compila en Windows.")
endif()

# ----------------------------------------------------------------------------
# Instalacion ligera para uso local
# ----------------------------------------------------------------------------
install(TARGETS lab RUNTIME DESTINATION .)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/results  DESTINATION . OPTIONAL)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/evidence DESTINATION . OPTIONAL)

# ----------------------------------------------------------------------------
# Mensaje de ayuda
# ----------------------------------------------------------------------------
message(STATUS "Laboratorio de procesos e hilos - Windows / C++17")
message(STATUS "  Binario: lab.exe")
message(STATUS "  Modo worker: lab.exe --worker --task <file> [--result <file>]")
message(STATUS "  Requiere: Windows 10/11, C++17, CMake 3.16+")
