#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <atomic>
#include "hardware_info.hpp"
#include "measurement.hpp"

namespace lab {

// Configuracion de una prueba concreta (hilos o procesos).
struct experiment_config {
  int workers;
  bool affinity = false;
  std::string experiment_name;
  std::string mode;
};

// Una fila de medicion. Los campos opcionales quedan en std::nullopt cuando la
// metrica no pudo obtenerse y se escriben como NA en el CSV.
struct experiment_result {
  int workers = 0;
  int repeat = 0;
  std::int64_t wall_ms = 0;
  std::int64_t cpu_ms = 0;
  std::int64_t worker_cpu_ms = 0;
  uint64_t verification_value = 0;
  bool verification_ok = false;
  std::optional<int> exit_code;
  std::optional<int> win32_error;
  std::string notes;
  bool affinity_requested = false;
  bool affinity_applied = false;
  std::optional<std::int64_t> peak_memory_bytes;
  std::string experiment_name;
  std::string mode;
  int logical_processors = 0;
  int physical_cores = 0;
  int iteration_count = 0;
  std::optional<double> cpu_utilization_percent; // CPU/wall * 100
};

struct experiment_context {
  hardware_info hw;
  uint64_t total_iterations = 0;
  int repetitions = 3;
  std::vector<int> thread_configs;
  std::vector<int> process_configs;
  std::string output_dir;
  bool skip_processes = false;
  bool skip_affinity = false;
  bool save_evidence = false; // conservar archivos temporales del worker
  std::int64_t timeout_ms = 120'000;
  std::atomic<bool>* cancel_flag = nullptr; // Ctrl+C: se revisa entre pruebas
  uint64_t baseline_wall_ms = 0;
  uint64_t baseline_cpu_ms = 0;
  uint64_t baseline_verification = 0;
  std::vector<experiment_result> results;
};

} // namespace lab
