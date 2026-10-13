#include "workload.hpp"
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>

namespace lab {

// ----------------------------------------------------------------------------
// Carga CPU-bound determinista
// ----------------------------------------------------------------------------
// SplitMix64: mezcla barata pero suficientemente costosa por iteracion para que
// el tiempo sea medible, sin asignaciones dinamicas ni E/S dentro del bucle.
// El estado NO depende de la iteracion previa, por lo que el resultado es
// aditivo y comparable entre particiones distintas del mismo rango total.
static inline std::uint64_t mix64(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

uint64_t cpu_work(uint64_t begin, uint64_t end) {
  std::uint64_t acc = 0; // acumulador puramente local: sin memoria compartida
  for (std::uint64_t i = begin; i < end; ++i) {
    // Se acumula en modulo 2^63 para que el valor de verificacion quepa en un
    // entero con signo de 64 bits y pueda serializarse en JSON sin ambiguedad.
    // La operacion sigue siendo aditiva respecto a la particion del rango.
    acc = (acc + mix64(i)) & 0x7FFFFFFFFFFFFFFFULL;
  }
  return acc;
}

uint64_t combine_verification(uint64_t partial_a, uint64_t partial_b) {
  // aditivo en modulo 2^63: refleja el trabajo total ejecutado
  return (partial_a + partial_b) & 0x7FFFFFFFFFFFFFFFULL;
}

// ----------------------------------------------------------------------------
// Reparto determinista de iteraciones
// ----------------------------------------------------------------------------
// base = total / workers; remainder = total % workers.
// Los primeros `remainder` workers reciben una iteracion extra, de modo que la
// suma de todas las particiones es exactamente total.
uint64_t worker_iterations(uint64_t total_iterations, int workers, int worker_index) {
  if (workers <= 0) return total_iterations;
  if (worker_index < 0 || worker_index >= workers) return 0;
  const std::uint64_t w = static_cast<std::uint64_t>(workers);
  const std::uint64_t base = total_iterations / w;
  const std::uint64_t remainder = total_iterations % w;
  return base + (static_cast<std::uint64_t>(worker_index) < remainder ? 1ULL : 0ULL);
}

void worker_range(uint64_t total_iterations, int workers, int worker_index,
                  uint64_t& begin, uint64_t& end) {
  const std::uint64_t w = static_cast<std::uint64_t>(workers <= 0 ? 1 : workers);

  // Desplazamiento = suma de las iteraciones de los workers anteriores.
  std::uint64_t offset = 0;
  for (int k = 0; k < worker_index; ++k) {
    offset += worker_iterations(total_iterations, workers, k);
  }
  begin = offset;
  end = begin + worker_iterations(total_iterations, workers, worker_index);
  if (worker_index >= workers - 1) {
    end = total_iterations; // el ultimo worker cierra el rango exactamente
  }
  (void)w;
}

// ----------------------------------------------------------------------------
// Worker task / result JSON helpers
// ----------------------------------------------------------------------------

worker_task parse_worker_task(const json::object& object) {
  worker_task t;
  if (const json::value* v = json::find(object, "begin")) {
    t.begin = static_cast<uint64_t>(v->as_int(0));
  }
  if (const json::value* v = json::find(object, "end")) {
    t.end = static_cast<uint64_t>(v->as_int(0));
  }
  if (const json::value* v = json::find(object, "output_path")) {
    t.output_path = v->as_string();
  }
  if (t.output_path.empty()) {
    t.output_path = "worker_result.json";
  }
  if (const json::value* v = json::find(object, "verify")) {
    t.verify = v->as_bool(true);
  }
  return t;
}

json::object make_worker_task_json(const worker_task& t) {
  json::object o;
  o["begin"] = json::value::from_int(static_cast<int64_t>(t.begin));
  o["end"] = json::value::from_int(static_cast<int64_t>(t.end));
  o["output_path"] = json::value::from_string(t.output_path);
  o["verify"] = json::value::from_bool(t.verify);
  return o;
}

json::object make_result_json(uint64_t begin, uint64_t end, uint64_t value,
                              int64_t wall_ms, int64_t cpu_ms, bool ok) {
  json::object o;
  o["worker_begin"] = json::value::from_int(static_cast<int64_t>(begin));
  o["worker_end"] = json::value::from_int(static_cast<int64_t>(end));
  o["value"] = json::value::from_int(static_cast<int64_t>(value));
  o["wall_ms"] = json::value::from_int(wall_ms);
  o["cpu_ms"] = json::value::from_int(cpu_ms);
  o["ok"] = json::value::from_bool(ok);
  return o;
}

worker_result parse_result_json(const json::object& object) {
  worker_result r;
  if (const json::value* v = json::find(object, "worker_begin")) r.begin = static_cast<uint64_t>(v->as_int(0));
  if (const json::value* v = json::find(object, "worker_end")) r.end = static_cast<uint64_t>(v->as_int(0));
  if (const json::value* v = json::find(object, "value")) r.verification_value = static_cast<uint64_t>(v->as_int(0));
  if (const json::value* v = json::find(object, "wall_ms")) r.wall_ms = v->as_int(0);
  if (const json::value* v = json::find(object, "cpu_ms")) r.cpu_ms = v->as_int(0);
  if (const json::value* v = json::find(object, "ok")) r.ok = v->as_bool(false);
  return r;
}

} // namespace lab
