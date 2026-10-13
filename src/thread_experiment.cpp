#include "thread_experiment.hpp"
#include "workload.hpp"
#include "measurement.hpp"
#include "terminal.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <thread>
#include <vector>
#include <atomic>
#include <string>
#include <sstream>

namespace lab {

// ----------------------------------------------------------------------------
// Helpers comunes
// ----------------------------------------------------------------------------

static void fill_common(experiment_result& r, const experiment_context& ctx,
                        int workers, int repeat, const std::string& name,
                        const std::string& mode) {
  r.workers = workers;
  r.repeat = repeat;
  r.iteration_count = static_cast<int>(ctx.total_iterations);
  r.physical_cores = ctx.hw.physical_cores;
  r.logical_processors = ctx.hw.logical_processors;
  r.experiment_name = name;
  r.mode = mode;
}

static void fill_timing(experiment_result& r, const measurement::timing& t) {
  r.wall_ms = measurement::to_ms(t.wall);
  r.cpu_ms = measurement::to_ms(t.process_cpu);
  r.worker_cpu_ms = measurement::to_ms(t.worker_cpu);
  r.cpu_utilization_percent = measurement::cpu_utilization_percent(r.cpu_ms, r.wall_ms);
  r.peak_memory_bytes = t.peak_memory_bytes;
}

// Marca una prueba como timeout si supero el limite configurado.
static void note_timeout(experiment_result& r, const experiment_context& ctx) {
  if (ctx.timeout_ms > 0 && r.wall_ms > ctx.timeout_ms) {
    r.verification_ok = false;
    r.exit_code = 1;
    r.notes += " timeout_excedido";
  }
}

// ----------------------------------------------------------------------------
// Ejecucion secuencial (linea base)
// ----------------------------------------------------------------------------

thread_experiment_run run_sequential(const experiment_context& ctx, int repeat) {
  thread_experiment_run run;
  fill_common(run.result, ctx, 1, repeat, "secuencial", "secuencial");

  measurement::timed_scope scope;
  const uint64_t value = cpu_work(0, ctx.total_iterations);
  scope.stop();
  fill_timing(run.result, scope.result());

  run.result.verification_value = value;
  run.result.verification_ok = true; // la secuencial define la referencia
  run.result.exit_code = 0;
  run.result.notes = "linea base; verificacion_ok=true";
  note_timeout(run.result, ctx);
  return run;
}

// ----------------------------------------------------------------------------
// Ejecucion paralela con hilos
// ----------------------------------------------------------------------------
// Cada hilo trabaja un rango contiguo y escribe en su propia posicion del
// arreglo (sin memoria compartida durante la ejecucion). La combinacion se hace
// despues del join. Como la carga es aditiva, la suma de parciales es igual al
// valor de la linea base.
// ----------------------------------------------------------------------------

thread_experiment_run run_thread_config(const experiment_context& ctx, int workers,
                                        int repeat, uint64_t expected_verification) {
  thread_experiment_run run;
  fill_common(run.result, ctx, workers, repeat, "hilos", std::to_string(workers) + "_hilos");

  std::vector<uint64_t> partials(static_cast<std::size_t>(workers), 0);
  std::vector<std::thread> threads;
  threads.reserve(static_cast<std::size_t>(workers));

  measurement::timed_scope scope;
  for (int i = 0; i < workers; ++i) {
    threads.emplace_back([total = ctx.total_iterations, workers, i, &partials]() {
      uint64_t begin = 0, end = 0;
      worker_range(total, workers, i, begin, end);
      partials[static_cast<std::size_t>(i)] = cpu_work(begin, end);
    });
  }
  for (auto& th : threads) th.join();
  scope.stop();

  uint64_t merged = 0;
  for (uint64_t v : partials) merged = combine_verification(merged, v);

  fill_timing(run.result, scope.result());
  run.result.verification_value = merged;
  run.result.verification_ok = (merged == expected_verification);
  run.result.exit_code = run.result.verification_ok ? 0 : 1;
  run.result.notes = run.result.verification_ok ? "verificacion_ok" : "verificacion_fallida";
  note_timeout(run.result, ctx);
  return run;
}

// ----------------------------------------------------------------------------
// Concurrencia con afinidad a un procesador logico
// ----------------------------------------------------------------------------
// Se crean varios hilos y se intenta restringir cada uno al procesador logico 0.
// Si la afinidad se aplica, los hilos comparten un unico procesador logico: hay
// concurrencia (avance intercalado) pero no paralelismo fisico simultaneo. Si
// SetThreadAffinityMask falla, se guarda el codigo de error de Windows y NO se
// afirma que la afinidad fue aplicada.
// ----------------------------------------------------------------------------

thread_experiment_run run_affinity_test(const experiment_context& ctx, int repeat,
                                        uint64_t expected_verification) {
  thread_experiment_run run;
  const int workers = 2;
  fill_common(run.result, ctx, workers, repeat, "concurrencia_afinidad", "afinidad");
  run.result.affinity_requested = true;

  std::vector<uint64_t> partials(static_cast<std::size_t>(workers), 0);
  std::vector<std::atomic<int>> affinity_status(static_cast<std::size_t>(workers));
  std::vector<std::atomic<DWORD>> affinity_error(static_cast<std::size_t>(workers));
  for (int i = 0; i < workers; ++i) {
    affinity_status[static_cast<std::size_t>(i)].store(0); // 0 = no aplicada
    affinity_error[static_cast<std::size_t>(i)].store(0);
  }

  measurement::timed_scope scope;
  std::vector<std::thread> threads;
  threads.reserve(static_cast<std::size_t>(workers));
  for (int i = 0; i < workers; ++i) {
    threads.emplace_back([total = ctx.total_iterations, workers, i, &partials,
                          &affinity_status, &affinity_error]() {
      // Intentar confinar el hilo actual al procesador logico 0.
      const DWORD_PTR mask = static_cast<DWORD_PTR>(1);
      if (SetThreadAffinityMask(GetCurrentThread(), mask) != 0) {
        affinity_status[static_cast<std::size_t>(i)].store(1);
      } else {
        affinity_error[static_cast<std::size_t>(i)].store(GetLastError());
      }
      uint64_t begin = 0, end = 0;
      worker_range(total, workers, i, begin, end);
      partials[static_cast<std::size_t>(i)] = cpu_work(begin, end);
    });
  }
  for (auto& th : threads) th.join();
  scope.stop();

  uint64_t merged = 0;
  for (uint64_t v : partials) merged = combine_verification(merged, v);

  bool applied = true;
  DWORD first_error = 0;
  for (int i = 0; i < workers; ++i) {
    if (affinity_status[static_cast<std::size_t>(i)].load() != 1) {
      applied = false;
      if (first_error == 0) first_error = affinity_error[static_cast<std::size_t>(i)].load();
    }
  }

  fill_timing(run.result, scope.result());
  run.result.verification_value = merged;
  run.result.verification_ok = (merged == expected_verification);
  run.result.affinity_applied = applied;
  if (!applied && first_error != 0) run.result.win32_error = static_cast<int>(first_error);
  run.result.exit_code = run.result.verification_ok ? 0 : 1;
  run.result.notes = applied
      ? "afinidad aplicada a LP 0; concurrencia sin paralelismo fisico"
      : "afinidad NO aplicada; ver win32_error_code";
  note_timeout(run.result, ctx);
  return run;
}

std::vector<thread_experiment_run> run_hilo_experiments(const experiment_context& ctx) {
  std::vector<thread_experiment_run> runs;

  // 1) linea base secuencial; su valor define la verificacion esperada.
  runs.push_back(run_sequential(ctx, 1));
  const uint64_t expected = runs.front().result.verification_value;

  // 2) configuraciones de hilos, repetidas.
  for (int w : ctx.thread_configs) {
    for (int r = 1; r <= ctx.repetitions; ++r) {
      if (ctx.cancel_flag && ctx.cancel_flag->load()) return runs;
      runs.push_back(run_thread_config(ctx, w, r, expected));
    }
  }

  // 3) prueba de concurrencia con afinidad.
  if (!ctx.skip_affinity && ctx.hw.logical_processors >= 2) {
    for (int r = 1; r <= ctx.repetitions; ++r) {
      if (ctx.cancel_flag && ctx.cancel_flag->load()) return runs;
      runs.push_back(run_affinity_test(ctx, r, expected));
    }
  }

  return runs;
}

} // namespace lab
