#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include <optional>

namespace lab::measurement {

// Reloj monotono para tiempo de pared (AGENT.md exige steady_clock).
using clock = std::chrono::steady_clock;

struct timing {
  std::chrono::nanoseconds wall;
  std::chrono::nanoseconds process_cpu;
  std::chrono::nanoseconds worker_cpu;
  std::optional<std::int64_t> peak_memory_bytes;
};

// Mide tiempo de pared y tiempo de CPU del proceso. El tiempo de CPU se toma de
// GetProcessTimes; si no esta disponible se deja en 0 y se debe registrar como
// NA en los resultados (nunca se inventa).
struct timed_scope {
  clock::time_point wall_start;
  clock::time_point wall_stop;
  std::chrono::nanoseconds process_cpu_start;
  std::chrono::nanoseconds process_cpu_stop;
  std::optional<std::int64_t> peak_memory;

  timed_scope();
  void stop();
  timing result() const;
};

std::int64_t to_ms(std::chrono::nanoseconds ns);
std::int64_t to_us(std::chrono::nanoseconds ns);

std::string format_duration_ms(std::int64_t ms);
std::string format_duration_us(std::int64_t us);

bool within_timeout(std::chrono::nanoseconds elapsed, std::int64_t timeout_ms);

// Utilizacion de CPU aproximada = tiempo de CPU / tiempo de pared * 100.
// Devuelve NA (std::nullopt) si no se puede calcular.
std::optional<double> cpu_utilization_percent(std::int64_t cpu_ms, std::int64_t wall_ms);

} // namespace lab::measurement
