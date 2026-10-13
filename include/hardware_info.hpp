#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "json.hpp"

namespace lab {

// Relacion entre un nucleo fisico y sus procesadores logicos.
struct cpu_core_info {
  int first_logical_processor; // primer procesador logico del nucleo
  int core_index;              // indice del nucleo fisico
  bool multi_threaded;         // true si el nucleo tiene mas de un hilo logico
};

struct hardware_info {
  std::string timestamp;
  std::string cpu_name;
  int physical_cpus = 0;      // sockets/packages habilitados (0 = NA)
  int physical_cores = 0;     // nucleos fisicos detectados (0 = NA)
  int logical_processors = 0; // procesadores logicos activos
  int processor_groups = 0;
  std::string arch_process;   // arquitectura del proceso (x86/x64/arm64...)
  std::string arch_system;    // arquitectura nativa del sistema
  std::string os_name;
  std::string compiler_id;
  std::int64_t total_memory_bytes = 0;     // 0 = NA
  std::int64_t available_memory_bytes = 0; // 0 = NA

  // Relacion nucleo fisico -> procesadores logicos (puede quedar vacia).
  std::vector<cpu_core_info> core_layout;

  // Configuracion elegida automaticamente
  std::vector<int> recommended_thread_configs;
  std::vector<int> recommended_process_configs;
  std::vector<std::string> thread_config_reasons;  // razon por configuracion de hilos
  std::vector<std::string> process_config_reasons; // razon por configuracion de procesos
  std::string limitations;
  std::string configuration_rationale;
};

// Detecta el hardware y calcula las configuraciones recomendadas.
hardware_info detect_hardware();

// Texto con la justificacion exacta de cada configuracion incluida/excluida.
std::string build_configuration_rationale(const hardware_info& hw, int lp_limit, int process_limit);

// Texto con las limitaciones detectadas (memoria, nucleos no detectados, etc.).
std::string build_limitations_text(const hardware_info& hw, int lp_limit, int process_limit);

// Serializacion para los archivos de resultados.
json::object hardware_to_json(const hardware_info& hw);
std::string hardware_to_csv(const hardware_info& hw);

} // namespace lab
