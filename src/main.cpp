#include "terminal.hpp"
#include "args.hpp"
#include "hardware_info.hpp"
#include "workload.hpp"
#include "measurement.hpp"
#include "csv_writer.hpp"
#include "json.hpp"
#include "experiment_context.hpp"
#include "thread_experiment.hpp"
#include "process_experiment.hpp"
#include "report_draft.hpp"

#include <windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <map>
#include <numeric>
#include <cmath>
#include <limits>
#include <cstdio>
#include <cstdint>
#include <atomic>
#include <thread>

namespace fs = std::filesystem;
using namespace lab;
using namespace lab::terminal;

// ----------------------------------------------------------------------------
// Cancelacion con Ctrl+C
// ----------------------------------------------------------------------------
namespace {
std::atomic<bool> g_cancelled{false};

BOOL WINAPI console_ctrl_handler(DWORD type) {
  if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
    g_cancelled.store(true);
    return TRUE; // el laboratorio termina ordenadamente entre pruebas
  }
  return FALSE;
}
} // namespace

// ----------------------------------------------------------------------------
// Utilidades de archivo
// ----------------------------------------------------------------------------

static std::string path_join(const std::string& a, const std::string& b) {
  return (fs::path(a) / b).string();
}

static bool read_file_text(const std::string& path, std::string& out) {
  std::ifstream f(path, std::ios::in | std::ios::binary);
  if (!f.is_open()) return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

static bool write_file_text(const std::string& path, const std::string& text) {
  std::ofstream f(path, std::ios::out | std::ios::trunc | std::ios::binary);
  if (!f.is_open()) return false;
  f.write(text.data(), static_cast<std::streamsize>(text.size()));
  return f.good() || f.eof();
}

static std::string fmt_list(const std::vector<int>& v) {
  if (v.empty()) return "(ninguna)";
  std::ostringstream ss;
  for (std::size_t i = 0; i < v.size(); ++i) {
    if (i) ss << ", ";
    ss << v[i];
  }
  return ss.str();
}

static std::string optional_int(const std::optional<int>& v, const std::string& na = "NA") {
  return v.has_value() ? std::to_string(*v) : na;
}

// ----------------------------------------------------------------------------
// Modo worker: ejecuta un rango y escribe su resultado en JSON.
// ----------------------------------------------------------------------------

static int run_worker(const program_options& opts, terminal::context& tctx) {
  if (opts.worker_task_path.empty()) {
    terminal::write_error(tctx, "Modo worker requiere --task <archivo.json>");
    return 1;
  }

  std::string content;
  if (!read_file_text(opts.worker_task_path, content)) {
    terminal::write_error(tctx, "No se pudo leer la tarea: " + opts.worker_task_path);
    return 1;
  }
  auto obj_opt = json::parse_object_from_string(content);
  if (!obj_opt) {
    terminal::write_error(tctx, "Formato de tarea invalido: " + opts.worker_task_path);
    return 1;
  }

  const worker_task task = parse_worker_task(*obj_opt);
  const std::string result_path = opts.worker_result_path.empty() ? task.output_path : opts.worker_result_path;

  terminal::write_info(tctx, "worker: begin=" + std::to_string(task.begin) +
                             " end=" + std::to_string(task.end));

  measurement::timed_scope scope;
  const uint64_t value = cpu_work(task.begin, task.end);
  scope.stop();
  const measurement::timing t = scope.result();

  auto result_obj = make_result_json(task.begin, task.end, value,
                                     measurement::to_ms(t.wall), measurement::to_ms(t.process_cpu), true);
  if (!write_file_text(result_path, json::serialize_object(result_obj) + "\n")) {
    terminal::write_error(tctx, "No se pudo escribir el resultado: " + result_path);
    return 1;
  }
  terminal::write_ok(tctx, "worker: resultado escrito en " + result_path);
  return 0;
}

// ----------------------------------------------------------------------------
// Serializacion de mediciones (JSON) con tipos correctos.
// ----------------------------------------------------------------------------

static json::value optional_int_value(const std::optional<int>& v) {
  return v.has_value() ? json::value::from_int(*v) : json::value::null_value();
}
static json::value optional_i64_value(const std::optional<std::int64_t>& v) {
  return v.has_value() ? json::value::from_int(*v) : json::value::null_value();
}
static json::value optional_double_value(const std::optional<double>& v) {
  return v.has_value() ? json::value::from_double(*v) : json::value::null_value();
}
static json::value optional_u64_value(const std::optional<std::uint64_t>& v) {
  return v.has_value() ? json::value::from_int(static_cast<std::int64_t>(*v)) : json::value::null_value();
}
static json::object run_to_json(int run_id, const experiment_result& r) {
  json::object o;
  o["run_id"] = json::value::from_int(run_id);
  o["experiment"] = json::value::from_string(r.experiment_name);
  o["mode"] = json::value::from_string(r.mode);
  o["workers"] = json::value::from_int(r.workers);
  o["logical_processors"] = json::value::from_int(r.logical_processors);
  o["physical_cores"] = json::value::from_int(r.physical_cores);
  o["iteration_count"] = json::value::from_int(r.iteration_count);
  o["repeat"] = json::value::from_int(r.repeat);
  o["wall_time_ms"] = json::value::from_int(r.wall_ms);
  o["process_cpu_time_ms"] = json::value::from_int(r.cpu_ms);
  o["worker_cpu_time_ms"] = json::value::from_int(r.worker_cpu_ms);
  o["verification_value"] = json::value::from_int(static_cast<std::int64_t>(r.verification_value));
  o["verification_ok"] = json::value::from_bool(r.verification_ok);
  o["affinity_requested"] = json::value::from_bool(r.affinity_requested);
  o["affinity_applied"] = json::value::from_bool(r.affinity_applied);
  o["affinity_mask"] = optional_u64_value(r.affinity_mask);
  o["peak_memory_bytes"] = optional_i64_value(r.peak_memory_bytes);
  o["exit_code"] = optional_int_value(r.exit_code);
  o["win32_error_code"] = optional_int_value(r.win32_error);
  o["cpu_utilization_percent"] = optional_double_value(r.cpu_utilization_percent);
  o["notes"] = json::value::from_string(r.notes);
  return o;
}

// ----------------------------------------------------------------------------
// Validaciones obligatorias (AGENT.md, seccion 11)
// ----------------------------------------------------------------------------

static bool validate_results(const std::vector<experiment_result>& results,
                             terminal::context& tctx) {
  bool ok = true;
  std::size_t verified = 0;
  for (const auto& r : results) {
    if (r.verification_ok) ++verified;
  }
  terminal::write_kv(tctx, "Ejecuciones verificadas",
                     std::to_string(verified) + "/" + std::to_string(results.size()));
  if (results.empty()) {
    terminal::write_error(tctx, "No se generaron resultados.");
    ok = false;
  }
  if (verified != results.size()) {
    terminal::write_warning(tctx, "Hay ejecuciones no verificadas; revisa la columna verification_ok.");
    ok = false;
  }
  return ok;
}

struct aggregate_stats {
  std::string experiment;
  std::string mode;
  int workers = 0;
  std::vector<double> wall_ms;
};

static double stats_mean(const std::vector<double>& values) {
  if (values.empty()) return 0.0;
  return std::accumulate(values.begin(), values.end(), 0.0) /
         static_cast<double>(values.size());
}

static double stats_stdev(const std::vector<double>& values) {
  if (values.size() < 3) return 0.0;
  const double average = stats_mean(values);
  double sum = 0.0;
  for (double value : values) {
    const double delta = value - average;
    sum += delta * delta;
  }
  return std::sqrt(sum / static_cast<double>(values.size() - 1));
}

static json::object make_stats_json(const aggregate_stats& group, double baseline_mean) {
  const double average = stats_mean(group.wall_ms);
  const double minimum = group.wall_ms.empty()
      ? 0.0 : *std::min_element(group.wall_ms.begin(), group.wall_ms.end());
  const double maximum = group.wall_ms.empty()
      ? 0.0 : *std::max_element(group.wall_ms.begin(), group.wall_ms.end());
  const double speedup = (baseline_mean > 0.0 && average > 0.0)
      ? baseline_mean / average : 0.0;
  json::object out;
  out["experiment"] = json::value::from_string(group.experiment);
  out["mode"] = json::value::from_string(group.mode);
  out["workers"] = json::value::from_int(group.workers);
  out["sample_count"] = json::value::from_int(static_cast<std::int64_t>(group.wall_ms.size()));
  out["mean_wall_ms"] = json::value::from_double(average);
  out["min_wall_ms"] = json::value::from_double(minimum);
  out["max_wall_ms"] = json::value::from_double(maximum);
  out["range_wall_ms"] = json::value::from_double(maximum - minimum);
  out["stdev_wall_ms"] = json::value::from_double(stats_stdev(group.wall_ms));
  out["speedup_vs_sequential"] = json::value::from_double(speedup);
  out["efficiency"] = json::value::from_double(group.workers > 0 ? speedup / group.workers : 0.0);
  out["improvement_percent"] = json::value::from_double(
      baseline_mean > 0.0 ? ((baseline_mean - average) / baseline_mean) * 100.0 : 0.0);
  return out;
}

static std::string stats_number_text(const json::value& value) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(6) << value.as_double();
  return stream.str();
}

static bool write_statistics_files(const std::string& output_dir,
                                  const std::vector<experiment_result>& results,
                                  const std::string& timestamp) {
  std::map<std::string, aggregate_stats> grouped;
  std::vector<double> baseline_samples;
  for (const auto& result : results) {
    if (!result.verification_ok) continue;
    if (result.experiment_name == "secuencial") {
      baseline_samples.push_back(static_cast<double>(result.wall_ms));
    }
    const std::string key = result.experiment_name + "\n" + result.mode + "\n" +
                            std::to_string(result.workers);
    auto& group = grouped[key];
    group.experiment = result.experiment_name;
    group.mode = result.mode;
    group.workers = result.workers;
    group.wall_ms.push_back(static_cast<double>(result.wall_ms));
  }
  const double baseline_mean = stats_mean(baseline_samples);

  const std::string csv_path = path_join(output_dir, "statistics.csv");
  csv::writer csv_out(csv_path);
  csv_out.set_header({
      "experiment", "mode", "workers", "sample_count", "mean_wall_ms",
      "min_wall_ms", "max_wall_ms", "range_wall_ms", "stdev_wall_ms",
      "speedup_vs_sequential", "efficiency", "improvement_percent"});

  json::object summary;
  summary["timestamp"] = json::value::from_string(timestamp);
  summary["baseline_mean_wall_ms"] = json::value::from_double(baseline_mean);
  json::array stats_array;
  for (const auto& entry : grouped) {
    const auto& group = entry.second;
    const json::object row_json = make_stats_json(group, baseline_mean);
    csv::record row;
    row.fields = {
        group.experiment,
        group.mode,
        std::to_string(group.workers),
        std::to_string(group.wall_ms.size()),
        stats_number_text(row_json.at("mean_wall_ms")),
        stats_number_text(row_json.at("min_wall_ms")),
        stats_number_text(row_json.at("max_wall_ms")),
        stats_number_text(row_json.at("range_wall_ms")),
        stats_number_text(row_json.at("stdev_wall_ms")),
        stats_number_text(row_json.at("speedup_vs_sequential")),
        stats_number_text(row_json.at("efficiency")),
        stats_number_text(row_json.at("improvement_percent"))};
    csv_out.write_record(std::move(row));
    stats_array.push_back(json::value::from_object(row_json));
  }
  csv_out.finish();
  summary["groups"] = json::value::from_array(std::move(stats_array));
  return write_file_text(path_join(output_dir, "statistics.json"),
                         json::serialize_object(summary) + "\n");
}

struct compatible_measurement_group {
  std::string mode;
  int units = 0;
  std::vector<double> wall_s;
  std::vector<double> cpu_s;
  std::vector<double> util_pct;
};

static std::string compatible_mode(const experiment_result& result) {
  if (result.experiment_name == "secuencial") return "secuencial";
  if (result.experiment_name == "hilos") return "hilos_windows";
  if (result.experiment_name == "procesos") return "procesos_windows";
  if (result.experiment_name == "concurrencia_afinidad") return "afinidad_1lp";
  return result.experiment_name;
}

static std::int64_t compatible_cpu_ms(const experiment_result& result) {
  // En Linux, procesos_fork usa RUSAGE_CHILDREN. En Windows, el equivalente
  // disponible en esta medicion es la suma del CPU reportado por los workers.
  return result.experiment_name == "procesos" ? result.worker_cpu_ms : result.cpu_ms;
}

static std::string fixed6(double value) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(6) << value;
  return stream.str();
}

static std::string fixed2(double value) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(2) << value;
  return stream.str();
}

static std::string fixed4(double value) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(4) << value;
  return stream.str();
}

static bool write_linux_compatible_outputs(const std::string& output_dir,
                                           const std::vector<experiment_result>& results) {
  const std::string measurements_path = path_join(output_dir, "mediciones.csv");
  csv::writer measurements(measurements_path);
  measurements.set_header({"rep", "mode", "units", "logical_cpus", "total_iters",
                           "wall_s", "cpu_s", "util_pct", "checksum"});
  std::map<std::string, compatible_measurement_group> groups;
  for (const auto& result : results) {
    if (!result.verification_ok) continue;
    const std::string mode = compatible_mode(result);
    const std::int64_t cpu_ms = compatible_cpu_ms(result);
    const double wall_s = static_cast<double>(result.wall_ms) / 1000.0;
    const double cpu_s = static_cast<double>(cpu_ms) / 1000.0;
    const double util_pct = wall_s > 0.0 ? (cpu_s / wall_s) * 100.0 : 0.0;
    csv::record row;
    row.fields = {std::to_string(result.repeat), mode, std::to_string(result.workers),
                  std::to_string(result.logical_processors),
                  std::to_string(result.iteration_count), fixed6(wall_s), fixed6(cpu_s),
                  fixed2(util_pct), std::to_string(result.verification_value)};
    measurements.write_record(std::move(row));

    const std::string key = mode + "\n" + std::to_string(result.workers);
    auto& group = groups[key];
    group.mode = mode;
    group.units = result.workers;
    group.wall_s.push_back(wall_s);
    group.cpu_s.push_back(cpu_s);
    group.util_pct.push_back(util_pct);
  }
  measurements.finish();

  double baseline_mean = 0.0;
  auto baseline_it = groups.find("secuencial\n1");
  if (baseline_it != groups.end()) baseline_mean = stats_mean(baseline_it->second.wall_s);

  const std::string statistics_path = path_join(output_dir, "estadisticas.csv");
  csv::writer statistics(statistics_path);
  statistics.set_header({"mode", "units", "n", "mean_s", "min_s", "max_s", "range_s",
                         "stdev_s", "mean_cpu_s", "mean_util_pct", "speedup",
                         "efficiency_pct"});
  for (const auto& entry : groups) {
    const auto& group = entry.second;
    const double average = stats_mean(group.wall_s);
    const double minimum = *std::min_element(group.wall_s.begin(), group.wall_s.end());
    const double maximum = *std::max_element(group.wall_s.begin(), group.wall_s.end());
    const double speedup = (baseline_mean > 0.0 && average > 0.0)
        ? baseline_mean / average : 0.0;
    csv::record row;
    row.fields = {group.mode, std::to_string(group.units), std::to_string(group.wall_s.size()),
                  fixed4(average), fixed4(minimum), fixed4(maximum), fixed4(maximum - minimum),
                  fixed4(stats_stdev(group.wall_s)), fixed4(stats_mean(group.cpu_s)),
                  fixed2(stats_mean(group.util_pct)), fixed4(speedup),
                  fixed2(group.units > 0 ? speedup / group.units * 100.0 : 0.0)};
    statistics.write_record(std::move(row));
  }
  statistics.finish();
  return true;
}

// ----------------------------------------------------------------------------
// main
// ----------------------------------------------------------------------------

int main(int argc, char** argv) {
  const program_options opts = parse_args(argc, argv);

  terminal::context tctx;
  tctx.cfg.colors = !opts.no_color;
  tctx.cfg.quiet = opts.quiet;
  tctx.prefix = "lab";
  tctx.show_timestamps = true;
  terminal::initialize(tctx.cfg.colors);

  SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

  if (opts.help) {
    terminal::write_line(tctx, usage_text(), color::cyan, false);
    return 0;
  }

  if (opts.worker) {
    return run_worker(opts, tctx);
  }

  terminal::set_title(tctx, "Laboratorio de Procesos e Hilos - Windows / C++17");
  terminal::write_section(tctx, "Inicio");
  terminal::write_info(tctx, "Sistema operativo objetivo: Windows 10/11 (64 bits)");
  terminal::write_info(tctx, "Lenguaje: C++17");
  terminal::write_info(tctx, "Presiona Ctrl+C para cancelar entre pruebas.");

  terminal::write_section(tctx, "Deteccion de hardware");
  const hardware_info hw = detect_hardware();

  terminal::write_kv(tctx, "CPU", hw.cpu_name);
  terminal::write_kv(tctx, "Procesadores logicos activos", std::to_string(hw.logical_processors));
  terminal::write_kv(tctx, "Nucleos fisicos detectados", hw.physical_cores > 0 ? std::to_string(hw.physical_cores) : "NA");
  terminal::write_kv(tctx, "Sockets fisicos", hw.physical_cpus > 0 ? std::to_string(hw.physical_cpus) : "NA");
  terminal::write_kv(tctx, "Grupos de procesadores", std::to_string(hw.processor_groups));
  terminal::write_kv(tctx, "Arquitectura proceso/sistema", hw.arch_process + " / " + hw.arch_system);
  terminal::write_kv(tctx, "Sistema operativo", hw.os_name);
  terminal::write_kv(tctx, "Compilador/version", hw.compiler_id);
  terminal::write_kv(tctx, "Memoria total (GB)", std::to_string(static_cast<double>(hw.total_memory_bytes) / 1073741824.0));
  terminal::write_kv(tctx, "Memoria disponible (GB)", std::to_string(static_cast<double>(hw.available_memory_bytes) / 1073741824.0));
  terminal::write_info(tctx, "PID de este proceso: " + std::to_string(GetCurrentProcessId()) +
                             " (buscalo en el Administrador de tareas).");
  if (hw.physical_cores == 0) {
    terminal::write_warning(tctx, "Nucleos fisicos no detectados; se registran como NA.");
  }

  // Directorio de salida y subdirectorios.
  const std::string output_dir = opts.output_dir.empty() ? "results" : opts.output_dir;
  fs::create_directories(output_dir);
  fs::create_directories(path_join(output_dir, "charts"));
  fs::create_directories(path_join(output_dir, "tmp"));

  // Escribir hardware.json y hardware.csv (Experimento 1).
  if (write_file_text(path_join(output_dir, "hardware.json"), json::serialize_object(hardware_to_json(hw)) + "\n")) {
    terminal::write_ok(tctx, "hardware.json: " + path_join(output_dir, "hardware.json"));
  } else {
    terminal::write_warning(tctx, "No se pudo escribir hardware.json");
  }
  if (write_file_text(path_join(output_dir, "hardware.csv"), hardware_to_csv(hw))) {
    terminal::write_ok(tctx, "hardware.csv: " + path_join(output_dir, "hardware.csv"));
  } else {
    terminal::write_warning(tctx, "No se pudo escribir hardware.csv");
  }

  // Configuraciones: vienen calculadas por detect_hardware; solo se aplican limites manuales.
  std::vector<int> thread_cfgs = hw.recommended_thread_configs;
  std::vector<int> process_cfgs = hw.recommended_process_configs;
  if (opts.max_workers > 0) {
    thread_cfgs.erase(std::remove_if(thread_cfgs.begin(), thread_cfgs.end(),
                                     [&](int w) { return w > opts.max_workers; }), thread_cfgs.end());
  }
  if (opts.max_processes > 0) {
    process_cfgs.erase(std::remove_if(process_cfgs.begin(), process_cfgs.end(),
                                      [&](int w) { return w > opts.max_processes; }), process_cfgs.end());
  }
  if (thread_cfgs.empty()) thread_cfgs.push_back(1);

  const int repetitions = opts.repetitions > 0 ? opts.repetitions : 3;
  const uint64_t total_iterations = opts.quick ? 1000000ULL : (opts.iterations > 0 ? opts.iterations : 50000000ULL);

  terminal::write_section(tctx, "Configuraciones");
  terminal::write_kv(tctx, "Hilos", fmt_list(thread_cfgs));
  terminal::write_kv(tctx, "Procesos", opts.skip_processes ? "skip" : fmt_list(process_cfgs));
  terminal::write_kv(tctx, "Iteraciones totales", std::to_string(total_iterations) + (opts.quick ? " (quick)" : ""));
  terminal::write_kv(tctx, "Repeticiones por configuracion", std::to_string(repetitions));
  terminal::write_kv(tctx, "Timeout por prueba (s)", std::to_string(opts.timeout_seconds));
  terminal::write_ok(tctx, "Preparado. Presiona Ctrl+C para cancelar.");
  std::this_thread::sleep_for(std::chrono::seconds(2));

  // Contexto de experimentos.
  experiment_context xctx;
  xctx.hw = hw;
  xctx.total_iterations = total_iterations;
  xctx.repetitions = repetitions;
  xctx.thread_configs = thread_cfgs;
  xctx.process_configs = process_cfgs;
  xctx.output_dir = output_dir;
  xctx.skip_processes = opts.skip_processes;
  xctx.skip_affinity = opts.skip_affinity;
  xctx.save_evidence = opts.save_evidence;
  xctx.timeout_ms = static_cast<std::int64_t>(opts.timeout_seconds) * 1000LL;
  xctx.cancel_flag = &g_cancelled;

  // Experimento 2/3/4/5: secuencial, hilos, afinidad y paralelismo.
  auto thread_runs = run_hilo_experiments(xctx);
  for (auto& tr : thread_runs) xctx.results.push_back(std::move(tr.result));

  if (!xctx.results.empty()) {
    xctx.baseline_wall_ms = static_cast<uint64_t>(xctx.results.front().wall_ms);
    xctx.baseline_cpu_ms = static_cast<uint64_t>(xctx.results.front().cpu_ms);
    xctx.baseline_verification = xctx.results.front().verification_value;
  }

  // Experimento 6: procesos.
  if (opts.skip_processes) {
    terminal::write_info(tctx, "Procesos: omitido por --skip-processes");
  } else if (g_cancelled.load()) {
    terminal::write_warning(tctx, "Cancelado por el usuario antes de los procesos.");
  } else {
    auto process_runs = run_process_experiments(xctx);
    for (auto& pr : process_runs) xctx.results.push_back(std::move(pr.result));
  }

  // Escritura de resultados.
  terminal::write_section(tctx, "Escribiendo mediciones");
  const std::string csv_path = path_join(output_dir, "measurements.csv");
  const std::string json_path = path_join(output_dir, "measurements.json");

  {
    csv::writer csv_out(csv_path);
    csv_out.set_header({
      "run_id", "experiment", "mode", "workers", "logical_processors", "physical_cores",
      "iteration_count", "repeat", "wall_time_ms", "process_cpu_time_ms", "worker_cpu_time_ms",
      "verification_value", "verification_ok", "affinity_requested", "affinity_applied",
      "affinity_mask", "peak_memory_bytes", "exit_code", "win32_error_code",
      "cpu_utilization_percent", "notes"
    });
    int run_id = 1;
    for (const auto& r : xctx.results) {
      csv::record row;
      row.fields.push_back(std::to_string(run_id++));
      row.fields.push_back(r.experiment_name);
      row.fields.push_back(r.mode);
      row.fields.push_back(std::to_string(r.workers));
      row.fields.push_back(std::to_string(r.logical_processors));
      row.fields.push_back(std::to_string(r.physical_cores));
      row.fields.push_back(std::to_string(r.iteration_count));
      row.fields.push_back(std::to_string(r.repeat));
      row.fields.push_back(std::to_string(r.wall_ms));
      row.fields.push_back(std::to_string(r.cpu_ms));
      row.fields.push_back(std::to_string(r.worker_cpu_ms));
      row.fields.push_back(std::to_string(r.verification_value));
      row.fields.push_back(r.verification_ok ? "TRUE" : "FALSE");
      row.fields.push_back(r.affinity_requested ? "TRUE" : "FALSE");
      row.fields.push_back(r.affinity_applied ? "TRUE" : "FALSE");
      row.fields.push_back(r.affinity_mask ? std::to_string(*r.affinity_mask) : "NA");
      row.fields.push_back(r.peak_memory_bytes ? std::to_string(*r.peak_memory_bytes) : "NA");
      row.fields.push_back(optional_int(r.exit_code));
      row.fields.push_back(optional_int(r.win32_error));
      row.fields.push_back(r.cpu_utilization_percent
                               ? [&] { std::ostringstream s; s << std::fixed << std::setprecision(2) << *r.cpu_utilization_percent; return s.str(); }()
                               : "NA");
      row.fields.push_back(r.notes);
      csv_out.write_record(std::move(row));
    }
    csv_out.finish();
  }
  terminal::write_ok(tctx, "CSV: " + csv_path);

  const bool statistics_written = write_statistics_files(output_dir, xctx.results, hw.timestamp);
  terminal::write_kv(tctx, "Estadisticas agregadas",
                     statistics_written ? "CSV/JSON escritos" : "no disponibles");
  const bool compatible_outputs_written =
      write_linux_compatible_outputs(output_dir, xctx.results);
  terminal::write_kv(tctx, "Formato compatible Linux",
                     compatible_outputs_written ? "mediciones.csv/estadisticas.csv escritos"
                                                 : "no disponibles");

  // measurements.json: mismo contenido que el CSV, con tipos correctos.
  {
    json::object summary;
    summary["timestamp"] = json::value::from_string(hw.timestamp);
    summary["hardware"] = json::value::from_object(hardware_to_json(hw));
    summary["baseline_wall_ms"] = json::value::from_int(static_cast<std::int64_t>(xctx.baseline_wall_ms));
    summary["baseline_verification_value"] = json::value::from_int(static_cast<std::int64_t>(xctx.baseline_verification));
    json::array runs_arr;
    int run_id = 1;
    for (const auto& r : xctx.results) {
      runs_arr.push_back(json::value::from_object(run_to_json(run_id++, r)));
    }
    summary["runs"] = json::value::from_array(std::move(runs_arr));
    if (!write_file_text(json_path, json::serialize_object(summary) + "\n")) {
      terminal::write_warning(tctx, "No se pudo escribir measurements.json");
    } else {
      terminal::write_ok(tctx, "JSON: " + json_path);
    }
  }

  // observations.txt: separa lo medido, lo que copia el estudiante y lo pendiente.
  {
    std::ostringstream obs;
    obs << "# Observaciones del laboratorio\n\n";
    obs << "## Datos medidos automaticamente\n";
    obs << "- Fecha y hora: " << hw.timestamp << "\n";
    obs << "- CPU: " << hw.cpu_name << "\n";
    obs << "- Procesadores logicos: " << hw.logical_processors << "\n";
    obs << "- Nucleos fisicos: " << (hw.physical_cores > 0 ? std::to_string(hw.physical_cores) : "NA") << "\n";
    obs << "- Sockets fisicos: " << (hw.physical_cpus > 0 ? std::to_string(hw.physical_cpus) : "NA") << "\n";
    obs << "- Linea base (wall): " << xctx.baseline_wall_ms << " ms\n";
    obs << "- Linea base (CPU proceso): " << xctx.baseline_cpu_ms << " ms\n";
    obs << "- Resultado de verificacion base: " << xctx.baseline_verification << "\n\n";
    obs << "## Datos copiados por el estudiante\n";
    obs << "- [ ] Pega aqui las capturas/notas del Administrador de tareas y de collect_windows_observations.ps1.\n\n";
    obs << "## Observaciones visuales pendientes\n";
    obs << "- [ ] Ver evidence/README.md y tomar las capturas listadas.\n";
    write_file_text(path_join(output_dir, "observations.txt"), obs.str());
  }

  // Borrador de informe.
  terminal::write_section(tctx, "Borrador de informe");
  const auto report = build_report_draft(xctx, csv_path, json_path, path_join(output_dir, "charts"));
  const std::string draft_path = path_join(output_dir, "report_draft.md");
  if (write_file_text(draft_path, render_report_markdown(report))) {
    terminal::write_ok(tctx, "Borrador: " + draft_path);
  } else {
    terminal::write_warning(tctx, "No se pudo escribir report_draft.md");
  }

  // Validacion final (relee y valida los archivos escritos).
  terminal::write_section(tctx, "Validacion final");
  bool files_readable = true;
  bool json_valid = false;
  {
    std::string tmp;
    if (!read_file_text(csv_path, tmp) || tmp.empty()) files_readable = false;
    if (!read_file_text(json_path, tmp) || tmp.empty()) {
      files_readable = false;
    } else {
      json_valid = static_cast<bool>(json::parse_object_from_string(tmp));
    }
    if (!read_file_text(path_join(output_dir, "statistics.csv"), tmp) || tmp.empty() ||
        !read_file_text(path_join(output_dir, "statistics.json"), tmp) || tmp.empty()) {
      files_readable = false;
    }
    if (!read_file_text(path_join(output_dir, "mediciones.csv"), tmp) || tmp.empty() ||
        !read_file_text(path_join(output_dir, "estadisticas.csv"), tmp) || tmp.empty()) {
      files_readable = false;
    }
  }
  const bool results_ok = validate_results(xctx.results, tctx);
  if (!statistics_written || !compatible_outputs_written) files_readable = false;
  terminal::write_kv(tctx, "CSV/JSON releibles", files_readable ? "si" : "no");
  terminal::write_kv(tctx, "measurements.json valido", json_valid ? "si" : "no");

  terminal::write_section(tctx, "Resumen final");
  terminal::write_kv(tctx, "Linea base (secuencial)", std::to_string(xctx.baseline_wall_ms) + " ms");
  terminal::write_kv(tctx, "CPU proceso (secuencial)", std::to_string(xctx.baseline_cpu_ms) + " ms");
  terminal::write_kv(tctx, "Verificacion base", std::to_string(xctx.baseline_verification));
  terminal::write_kv(tctx, "Archivos",
                     "hardware.*, measurements.*, statistics.*, mediciones.csv, estadisticas.csv, "
                     "observations.txt, report_draft.md");
  if (opts.quick) {
    terminal::write_info(tctx, "Modo rapido: resultados no validos para el informe final.");
  }
  if (g_cancelled.load()) {
    terminal::write_warning(tctx, "Ejecucion cancelada; los resultados parciales quedaron guardados.");
  } else if (results_ok && files_readable && json_valid) {
    terminal::write_ok(tctx, "Ejecucion completa. Revisa results/ y las graficas antes de redactar.");
  } else {
    terminal::write_warning(tctx, "Ejecucion completa con advertencias; revisa los mensajes anteriores.");
  }

  return 0;
}
