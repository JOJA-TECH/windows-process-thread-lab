#pragma once
#include <vector>
#include "experiment_context.hpp"

namespace lab {

// Envuelve una fila de medicion para las pruebas de hilos. Se mantiene el mismo
// tipo que usa process_experiment para poder agregarlas en un solo vector.
struct thread_experiment_run {
  experiment_result result;
};

// Linea base: un solo flujo de trabajo sobre [0, total_iterations).
thread_experiment_run run_sequential(const experiment_context& ctx, int repeat);

// Configuracion con `workers` hilos y particion contigua del trabajo total.
// `expected_verification` es el valor de la linea base; la prueba se marca como
// fallida si el valor combinado no coincide.
thread_experiment_run run_thread_config(const experiment_context& ctx, int workers,
                                        int repeat, uint64_t expected_verification);

// Concurrencia restringida a un unico procesador logico mediante afinidad.
thread_experiment_run run_affinity_test(const experiment_context& ctx, int repeat,
                                        uint64_t expected_verification);

// Ejecuta linea base + configuraciones de hilos + prueba de afinidad.
std::vector<thread_experiment_run> run_hilo_experiments(const experiment_context& ctx);

} // namespace lab
