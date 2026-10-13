#include "args.hpp"
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>
#include <algorithm>

namespace lab {

static std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

// Reservado para un futuro formato --clave=valor; [[maybe_unused]] evita el
// aviso de funcion sin usar bajo -Wall.
[[maybe_unused]] static std::string trim(std::string_view s) {
  std::size_t i = 0;
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
  std::size_t j = s.size();
  while (j > i && std::isspace(static_cast<unsigned char>(s[j - 1]))) --j;
  return std::string(s.substr(i, j - i));
}

program_options parse_args(int argc, char** argv) {
  program_options opts;
  std::vector<std::string> args(argv + 1, argv + argc);

  for (std::size_t i = 0; i < args.size(); ++i) {
    std::string arg = args[i];
    std::string lower = to_lower(arg);

    if (lower == "--help" || lower == "-h") {
      opts.help = true;
      continue;
    }

    if (lower == "--worker") {
      opts.worker = true;
      continue;
    }

    if (lower == "--output" || lower == "-o") {
      if (i + 1 < args.size()) {
        opts.output_dir = args[++i];
      } else {
        std::cerr << "Falta valor para " << arg << "\n";
      }
      continue;
    }

    if (lower == "--iterations" || lower == "-n") {
      if (i + 1 < args.size()) {
        char* end = nullptr;
        auto val = std::strtoull(args[++i].c_str(), &end, 10);
        if (end == args[i].c_str() || *end != '\0') {
          std::cerr << "Valor invalido para --iterations: " << args[i] << "\n";
        } else {
          opts.iterations = val;
        }
      } else {
        std::cerr << "Falta valor para " << arg << "\n";
      }
      continue;
    }

    if (lower == "--repetitions" || lower == "-r") {
      if (i + 1 < args.size()) {
        char* end = nullptr;
        auto val = std::strtol(args[++i].c_str(), &end, 10);
        if (end == args[i].c_str() || *end != '\0' || val < 1) {
          std::cerr << "Valor invalido para --repetitions: " << args[i] << "\n";
        } else {
          opts.repetitions = static_cast<int>(val);
        }
      } else {
        std::cerr << "Falta valor para " << arg << "\n";
      }
      continue;
    }

    if (lower == "--max-workers") {
      if (i + 1 < args.size()) {
        char* end = nullptr;
        auto val = std::strtol(args[++i].c_str(), &end, 10);
        if (end == args[i].c_str() || *end != '\0') {
          std::cerr << "Valor invalido para --max-workers: " << args[i] << "\n";
        } else {
          opts.max_workers = static_cast<int>(val);
        }
      } else {
        std::cerr << "Falta valor para " << arg << "\n";
      }
      continue;
    }

    if (lower == "--max-processes") {
      if (i + 1 < args.size()) {
        char* end = nullptr;
        auto val = std::strtol(args[++i].c_str(), &end, 10);
        if (end == args[i].c_str() || *end != '\0') {
          std::cerr << "Valor invalido para --max-processes: " << args[i] << "\n";
        } else {
          opts.max_processes = static_cast<int>(val);
        }
      } else {
        std::cerr << "Falta valor para " << arg << "\n";
      }
      continue;
    }

    if (lower == "--timeout-seconds" || lower == "-t") {
      if (i + 1 < args.size()) {
        char* end = nullptr;
        auto val = std::strtol(args[++i].c_str(), &end, 10);
        if (end == args[i].c_str() || *end != '\0' || val < 1) {
          std::cerr << "Valor invalido para --timeout-seconds: " << args[i] << "\n";
        } else {
          opts.timeout_seconds = static_cast<int>(val);
        }
      } else {
        std::cerr << "Falta valor para " << arg << "\n";
      }
      continue;
    }

    if (lower == "--skip-processes") {
      opts.skip_processes = true;
      continue;
    }

    if (lower == "--skip-affinity") {
      opts.skip_affinity = true;
      continue;
    }

    if (lower == "--quick" || lower == "-q") {
      opts.quick = true;
      continue;
    }

    if (lower == "--no-color") {
      opts.no_color = true;
      continue;
    }

    if (lower == "--quiet" || lower == "-Q") {
      opts.quiet = true;
      continue;
    }

    if (lower == "--save-evidence") {
      opts.save_evidence = true;
      continue;
    }

    if (lower == "--task") {
      if (i + 1 < args.size()) {
        opts.worker_task_path = args[++i];
      } else {
        std::cerr << "Falta valor para " << arg << "\n";
      }
      continue;
    }

    if (lower == "--result") {
      if (i + 1 < args.size()) {
        opts.worker_result_path = args[++i];
      } else {
        std::cerr << "Falta valor para " << arg << "\n";
      }
      continue;
    }

    if (lower == "--affinity-cpu") {
      if (i + 1 < args.size()) {
        opts.affinity_cpu = args[++i];
      } else {
        std::cerr << "Falta valor para " << arg << "\n";
      }
      continue;
    }

    // Posicional o desconocido
    if (!arg.empty() && arg[0] != '-') {
      opts.positional.push_back(arg);
    } else {
      std::cerr << "Argumento no reconocido: " << arg << "\n";
    }
  }

  return opts;
}

std::string usage_text() {
  return R"(
lab.exe — Laboratorio de procesos e hilos (Windows / C++17)

USO:
  lab.exe [opciones]
  lab.exe --worker --task <archivo.json> [--result <archivo.json>]

MODOS:
  Por defecto, el programa actua como orquestador:
    - deteccion de hardware
    - linea base secuencial
    - hilos (std::thread)
    - concurrencia con afinidad (Win32)
    - paralelismo con varios procesadores
    - procesos (CreateProcessW) cuando no se salta

  --worker ejecuta un trabajador individual:
    - lee la tarea desde --task
    - ejecuta la misma carga CPU-bound
    - escribe su resultado en --result (o la ruta dentro de la tarea)

OPCIONES PRINCIPALES:
  --help, -h                 Muestra esta ayuda.
  --output, -o <dir>         Directorio de resultados (por defecto: results).
  --iterations, -n <n>      Iteraciones totales de carga por ejecucion.
  --repetitions, -r <n>     Repeticiones por configuracion (por defecto: 3).
  --max-workers <n>         Limite manual para hilos (automatico si no se usa).
  --max-processes <n>       Limite manual para procesos (automatico si no se usa).
  --timeout-seconds, -t <n> Tiempo maximo por prueba en segundos (por defecto: 120).
  --quick, -q               Modo funcional rapido (no es resultado de laboratorio).
  --skip-processes          Salta el experimento de procesos.
  --skip-affinity           Salta la prueba de afinidad (concurrencia restringida).
  --save-evidence           Conserva archivos temporales del worker como evidencia.
  --no-color                Desactiva colores ANSI.
  --quiet, -Q               Minimal output (para ejecucion automatica).
  --worker                  Ejecuta en modo trabajador.
  --task <file>             (worker) archivo de tarea JSON de entrada.
  --result <file>           (worker) archivo de resultado JSON de salida.

NOTAS:
  - El programa detecta automaticamente procesadores logicos y limita las pruebas.
  - Las configuraciones se registran en hardware.csv con la razon de inclusion/exclusion.
  - Si una metrica no puede obtenerse sin herramientas administrativas, se registra como NA.
)";
}

} // namespace lab
