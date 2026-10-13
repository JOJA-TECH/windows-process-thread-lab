#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include "json.hpp"

namespace lab {

// ----------------------------------------------------------------------------
// Carga determinista del laboratorio
// ----------------------------------------------------------------------------
// cpu_work(begin, end) recorre el rango [begin, end) y acumula un checksum.
// El checksum es ADITIVO respecto a la particion del rango: la suma de
// cpu_work() sobre rangos contiguos que cubren [0, N) es exactamente igual a
// cpu_work(0, N). Esto es lo que permite comparar el valor de verificacion de
// la version secuencial con la de hilos y la de procesos aunque el trabajo se
// reparta de forma distinta.
//
// La misma funcion se usa en secuencial, hilos y procesos para mantener la
// carga total constante entre configuraciones.
uint64_t cpu_work(uint64_t begin, uint64_t end);

// Combina los valores de verificacion de varios trabajadores. Al ser la carga
// aditiva, la combinacion correcta es la suma (con desbordamiento modular de
// 64 bits, que es determinista).
uint64_t combine_verification(uint64_t partial_a, uint64_t partial_b);

// Iteraciones que ejecuta el worker `worker_index` de `workers` para repartir
// `total_iterations` (el resto se reparte a los primeros indices).
uint64_t worker_iterations(uint64_t total_iterations, int workers, int worker_index);

// Desplazamiento [begin, end) de un worker concreto. Garantiza que la union de
// todos los rangos es exactamente [0, total_iterations).
void worker_range(uint64_t total_iterations, int workers, int worker_index,
                  uint64_t& begin, uint64_t& end);

// ----------------------------------------------------------------------------
// Formato de archivo de tarea / resultado del worker (modo --worker)
// ----------------------------------------------------------------------------

struct worker_task {
  uint64_t begin = 0;
  uint64_t end = 0;
  std::string output_path;
  bool verify = true;
};

worker_task parse_worker_task(const json::object& object);
json::object make_worker_task_json(const worker_task& t);

json::object make_result_json(uint64_t begin, uint64_t end, uint64_t value,
                              int64_t wall_ms, int64_t cpu_ms, bool ok);

struct worker_result {
  uint64_t begin = 0;
  uint64_t end = 0;
  uint64_t verification_value = 0;
  int64_t wall_ms = 0;
  int64_t cpu_ms = 0;
  bool ok = false;
};

worker_result parse_result_json(const json::object& object);

} // namespace lab
