#include "report_draft.hpp"
#include "csv_writer.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <fstream>

namespace lab {

// ----------------------------------------------------------------------------
// Estadisticas basicas
// ----------------------------------------------------------------------------

static double mean(const std::vector<double>& v) {
  if (v.empty()) return 0.0;
  double sum = std::accumulate(v.begin(), v.end(), 0.0);
  return sum / static_cast<double>(v.size());
}

static double stdev(const std::vector<double>& v) {
  if (v.size() < 3) return 0.0; // desviacion solo si hay >= 3 repeticiones (AGENT.md)
  const double m = mean(v);
  double sq = 0.0;
  for (double x : v) sq += (x - m) * (x - m);
  return std::sqrt(sq / static_cast<double>(v.size() - 1));
}

static double min_of(const std::vector<double>& v) {
  return v.empty() ? 0.0 : *std::min_element(v.begin(), v.end());
}

static double max_of(const std::vector<double>& v) {
  return v.empty() ? 0.0 : *std::max_element(v.begin(), v.end());
}

// ----------------------------------------------------------------------------
// Reporte
// ----------------------------------------------------------------------------

static std::string escape_md_cell(std::string s) {
  std::string out;
  out.reserve(s.size() + 16);
  for (char c : s) {
    if (c == '\n' || c == '\r') {
      out += ' ';
    } else if (c == '|' || c == '\\') {
      out += '\\';
      out += c;
    } else {
      out += c;
    }
  }
  return out;
}

static std::string fixed2(double v) {
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(2) << v;
  return ss.str();
}

// Agrupa los tiempos de pared por numero de trabajadores para un experimento.
static std::map<int, std::vector<double>> group_wall_by_workers(
    const experiment_context& ctx, const std::string& experiment) {
  std::map<int, std::vector<double>> groups;
  for (const auto& r : ctx.results) {
    if (r.experiment_name == experiment && r.verification_ok) {
      groups[r.workers].push_back(static_cast<double>(r.wall_ms));
    }
  }
  return groups;
}

report_blocks build_report_draft(const experiment_context& ctx, std::string measurements_csv,
                                  std::string measurements_json, std::string charts_dir) {
  report_blocks rb;

  rb.objective =
      "Comparar el comportamiento de ejecucion secuencial, con hilos y con procesos "
      "en Windows para analizar concurrencia, paralelismo y planificacion de CPU, "
      "usando una carga CPU-bound deterministica y repeticiones multiples.";

  rb.assumptions =
      "Sistema operativo: Windows 10/11 de 64 bits. Compilador: " + ctx.hw.compiler_id + ". "
      "La carga usada en secuencial, hilos y procesos es la misma (resultado de verificacion "
      "aditivo y comparable). Los resultados deben generarse en el equipo del estudiante; "
      "este borrador no inventa mediciones.";

  std::ostringstream hw_char;
  hw_char << "- Nombre de CPU: " << ctx.hw.cpu_name << "\n"
          << "- Procesadores logicos activos: " << ctx.hw.logical_processors << "\n"
          << "- Nucleos fisicos detectados: " << (ctx.hw.physical_cores > 0 ? std::to_string(ctx.hw.physical_cores) : "NA") << "\n"
          << "- Sockets/procesadores fisicos: " << (ctx.hw.physical_cpus > 0 ? std::to_string(ctx.hw.physical_cpus) : "NA") << "\n"
          << "- Grupos de procesadores: " << ctx.hw.processor_groups << "\n"
          << "- Arquitectura del proceso: " << ctx.hw.arch_process << "\n"
          << "- Arquitectura del sistema: " << ctx.hw.arch_system << "\n"
          << "- Sistema operativo: " << ctx.hw.os_name << "\n"
          << "- Compilador/version: " << ctx.hw.compiler_id << "\n"
          << "- Memoria total (bytes): " << ctx.hw.total_memory_bytes << "\n"
          << "- Memoria disponible (bytes): " << ctx.hw.available_memory_bytes << "\n";
  rb.hardware_characterization = hw_char.str();

  std::ostringstream proc;
  proc << "1. Deteccion de hardware automatica y exportacion a hardware.json/hardware.csv.\n"
       << "2. Ejecucion secuencial como linea base.\n"
       << "3. Experimentos con hilos: " << ctx.thread_configs.size() << " configuraciones.\n"
       << "4. Prueba de concurrencia con afinidad a un procesador logico.\n"
       << "5. Prueba de paralelismo con varios procesadores logicos.\n"
       << "6. Experimento de procesos (CreateProcessW + archivos de tarea/resultado).\n"
       << "7. Repeticiones por configuracion: " << ctx.repetitions << ".\n"
       << "8. Exportacion a measurements.csv y measurements.json.\n"
       << "9. Graficas generadas a partir de los datos reales (scripts/make_charts.py).\n"
       << "10. Borrador de informe con hallazgos automaticos cautelosos.\n";
  rb.procedure = proc.str();

  // Tabla de configuraciones realmente ejecutadas.
  std::ostringstream table;
  table << "| experiment | mode | workers | repeat | wall_ms | cpu_ms | worker_cpu_ms | cpu_util_% | verify_ok | affinity | notes |\n"
        << "|------------|------|---------|--------|---------|--------|---------------|------------|-----------|----------|-------|\n";
  for (const auto& r : ctx.results) {
    table << "| " << escape_md_cell(r.experiment_name) << " | "
          << escape_md_cell(r.mode) << " | " << r.workers << " | " << r.repeat << " | "
          << r.wall_ms << " | " << r.cpu_ms << " | " << r.worker_cpu_ms << " | "
          << (r.cpu_utilization_percent ? fixed2(*r.cpu_utilization_percent) : "NA") << " | "
          << (r.verification_ok ? "OK" : "FAIL") << " | "
          << (r.affinity_applied ? "yes" : (r.affinity_requested ? "requested" : "-")) << " | "
          << escape_md_cell(r.notes) << " |\n";
  }
  rb.configuration_table = table.str();

  // Estadisticas por configuracion de hilos.
  const auto thread_groups = group_wall_by_workers(ctx, "hilos");
  std::vector<double> baseline_samples;
  for (const auto& r : ctx.results) {
    if (r.experiment_name == "secuencial" && r.verification_ok) {
      baseline_samples.push_back(static_cast<double>(r.wall_ms));
    }
  }
  const double baseline_mean = mean(baseline_samples);

  std::ostringstream stats;
  stats << "| workers | n | media_ms | min_ms | max_ms | rango_ms | sd_ms | speedup | eficiencia | mejora_% |\n"
        << "|---------|---|----------|--------|--------|----------|-------|---------|------------|----------|\n";
  for (const auto& kv : thread_groups) {
    const std::vector<double>& v = kv.second;
    const double m = mean(v);
    const double speedup = (m > 0.0 && baseline_mean > 0.0) ? baseline_mean / m : 0.0;
    const double efficiency = (kv.first > 0) ? speedup / kv.first : 0.0;
    const double improvement = (baseline_mean > 0.0) ? ((baseline_mean - m) / baseline_mean) * 100.0 : 0.0;
    stats << "| " << kv.first << " | " << v.size() << " | " << fixed2(m) << " | "
          << fixed2(min_of(v)) << " | " << fixed2(max_of(v)) << " | "
          << fixed2(max_of(v) - min_of(v)) << " | " << fixed2(stdev(v)) << " | "
          << fixed2(speedup) << " | " << fixed2(efficiency) << " | " << fixed2(improvement) << " |\n";
  }

  std::ostringstream res;
  res << "Linea base secuencial (media de " << baseline_samples.size() << " repeticion(es)): "
      << fixed2(baseline_mean) << " ms de tiempo de pared.\n\n"
      << "Estadisticas por numero de hilos (speedup = linea base / media; eficiencia = speedup / hilos):\n\n"
      << stats.str() << "\n"
      << "Las mediciones completas estan en `" << measurements_csv << "` y `" << measurements_json << "`.\n";
  rb.results = res.str();

  // Graficas: se enlazan los archivos que produce make_charts.py.
  std::ostringstream charts;
  charts << "Graficas generadas en `" << charts_dir << "` a partir de measurements.csv:\n\n"
         << "![Tiempo vs hilos](" << charts_dir << "/wall_time_by_threads.png)\n\n"
         << "![Speedup vs hilos](" << charts_dir << "/speedup_by_threads.png)\n\n"
         << "![Eficiencia vs hilos](" << charts_dir << "/efficiency_by_threads.png)\n\n"
         << "![Hilos vs procesos](" << charts_dir << "/threads_vs_processes.png)\n\n"
         << "![Uso de CPU](" << charts_dir << "/cpu_utilization.png)\n\n"
         << "Ejecuta `python scripts/make_charts.py --csv " << measurements_csv
         << " --out " << charts_dir << "` si aun no existen.\n";
  rb.charts = charts.str();

  std::ostringstream findings;
  findings << "Hallazgos automaticos (hipotesis a contrastar con datos reales):\n\n";
  for (const auto& kv : thread_groups) {
    const std::vector<double>& v = kv.second;
    const double m = mean(v);
    const double speedup = (m > 0.0 && baseline_mean > 0.0) ? baseline_mean / m : 0.0;
    findings << "- Hilos = " << kv.first << ": media ~" << fixed2(m) << " ms (sd " << fixed2(stdev(v))
             << " ms, n=" << v.size() << "), speedup ~" << fixed2(speedup) << ".\n";
  }
  findings << "\nInterpretar con cautela: el scheduler de Windows puede migrar hilos entre nucleos; "
              "no se afirma que un hilo permanecio en un nucleo concreto sin medicion de afinidad valida.\n";
  rb.findings = findings.str();

  std::ostringstream limits;
  limits << "- Nucleos fisicos: " << (ctx.hw.physical_cores > 0 ? std::to_string(ctx.hw.physical_cores) : "NA") << ".\n"
         << "- Tiempos de CPU del proceso pueden incluir overhead de medicion.\n"
         << "- worker_cpu_ms de procesos es la suma del CPU reportado por cada hijo; puede subestimar el total.\n"
         << "- La afinidad solo se marca como aplicada si SetThreadAffinityMask tuvo exito.\n"
         << "- El modo worker usa archivos temporales; problemas de escritura se registran como fallos.\n"
         << "- Los tiempos dependen del equipo del estudiante y no pueden generalizarse.\n";
  rb.limitations = limits.str();

  std::ostringstream questions;
  questions << "1. ¿Que relacion hay entre el numero de nucleos fisicos y el speedup observado?\n"
            << "2. ¿En que configuraciones la eficiencia cae y por que podria ocurrir?\n"
            << "3. ¿Como se comporta el scheduler cuando se crean mas hilos que procesadores logicos?\n"
            << "4. ¿Que diferencias se observan entre hilos y procesos en tiempo y uso de CPU?\n"
            << "5. ¿Que evidencia grafica o de Administrador de tareas respalda las conclusiones?\n"
            << "6. ¿Que limitaciones impiden una conclusion mas firme sobre paralelismo fisico?\n";
  rb.questions = questions.str();

  std::ostringstream prelim;
  prelim << "- La linea base secuencial tardo " << fixed2(baseline_mean) << " ms (media).\n"
         << "- Los datos sugieren las tendencias anteriores, pero el analisis final debe basarse en "
            "las mediciones y capturas del estudiante.\n";
  rb.preliminary_conclusions = prelim.str();

  rb.student_fields =
      "- Fecha y hora de ejecucion local.\n"
      "- Observaciones de Administrador de tareas.\n"
      "- Interpretacion propia de cada grafica.\n"
      "- Respuestas a las preguntas del laboratorio.\n"
      "- Conclusiones finales.\n";

  return rb;
}

std::string render_report_markdown(const report_blocks& blocks) {
  std::ostringstream md;
  md << "# Informe de laboratorio: Procesos e Hilos en Windows\n\n"
     << "## Objetivo\n\n" << blocks.objective << "\n\n"
     << "## Supuestos\n\n" << blocks.assumptions << "\n\n"
     << "## Caracterizacion del equipo\n\n" << blocks.hardware_characterization << "\n\n"
     << "## Procedimiento\n\n" << blocks.procedure << "\n\n"
     << "## Configuraciones ejecutadas\n\n" << blocks.configuration_table << "\n\n"
     << "## Resultados\n\n" << blocks.results << "\n\n"
     << "## Graficas\n\n" << blocks.charts << "\n\n"
     << "## Hallazgos automaticos\n\n" << blocks.findings << "\n\n"
     << "## Limitaciones\n\n" << blocks.limitations << "\n\n"
     << "## Preguntas del laboratorio\n\n" << blocks.questions << "\n\n"
     << "## Conclusiones preliminares\n\n" << blocks.preliminary_conclusions << "\n\n"
     << "## Campos para completar por el estudiante\n\n" << blocks.student_fields << "\n";
  return md.str();
}

} // namespace lab
