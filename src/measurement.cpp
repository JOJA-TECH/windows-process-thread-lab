#include "measurement.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <psapi.h>
#include <cstdio>
#include <string>

namespace lab::measurement {

// Lee el tiempo de CPU (kernel + user) del proceso actual en nanosegundos.
// Devuelve false si GetProcessTimes no esta disponible.
static bool read_process_cpu_ns(std::chrono::nanoseconds& out) {
  FILETIME ftCreate{}, ftExit{}, ftKernel{}, ftUser{};
  if (!GetProcessTimes(GetCurrentProcess(), &ftCreate, &ftExit, &ftKernel, &ftUser)) {
    out = std::chrono::nanoseconds(0);
    return false;
  }
  ULARGE_INTEGER kernel{}, user{};
  kernel.LowPart = ftKernel.dwLowDateTime;
  kernel.HighPart = ftKernel.dwHighDateTime;
  user.LowPart = ftUser.dwLowDateTime;
  user.HighPart = ftUser.dwHighDateTime;
  // FILETIME esta en unidades de 100 ns.
  out = std::chrono::nanoseconds(
      static_cast<long long>((kernel.QuadPart + user.QuadPart) * 100ULL));
  return true;
}

timed_scope::timed_scope() : wall_start(clock::now()) {
  read_process_cpu_ns(process_cpu_start);
  peak_memory = std::nullopt;
}

void timed_scope::stop() {
  wall_stop = clock::now();
  read_process_cpu_ns(process_cpu_stop);

  PROCESS_MEMORY_COUNTERS_EX pmc{};
  pmc.cb = sizeof(pmc);
  if (GetProcessMemoryInfo(GetCurrentProcess(),
                           reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
    peak_memory = static_cast<std::int64_t>(pmc.PeakWorkingSetSize);
  }
}

timing timed_scope::result() const {
  timing t;
  t.wall = std::chrono::duration_cast<std::chrono::nanoseconds>(wall_stop - wall_start);
  t.process_cpu = std::chrono::duration_cast<std::chrono::nanoseconds>(process_cpu_stop - process_cpu_start);
  t.worker_cpu = std::chrono::nanoseconds(0);
  t.peak_memory_bytes = peak_memory;
  return t;
}

std::int64_t to_ms(std::chrono::nanoseconds ns) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(ns).count();
}

std::int64_t to_us(std::chrono::nanoseconds ns) {
  return std::chrono::duration_cast<std::chrono::microseconds>(ns).count();
}

std::string format_duration_ms(std::int64_t ms) {
  if (ms < 1000) return std::to_string(ms) + " ms";
  double s = static_cast<double>(ms) / 1000.0;
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.3f s", s);
  return buf;
}

std::string format_duration_us(std::int64_t us) {
  if (us < 1000) return std::to_string(us) + " us";
  double ms = static_cast<double>(us) / 1000.0;
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.3f ms", ms);
  return buf;
}

bool within_timeout(std::chrono::nanoseconds elapsed, std::int64_t timeout_ms) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() <= timeout_ms;
}

std::optional<double> cpu_utilization_percent(std::int64_t cpu_ms, std::int64_t wall_ms) {
  if (wall_ms <= 0 || cpu_ms < 0) return std::nullopt;
  return (static_cast<double>(cpu_ms) / static_cast<double>(wall_ms)) * 100.0;
}

} // namespace lab::measurement
