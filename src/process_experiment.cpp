#include "process_experiment.hpp"
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
#include <algorithm>
#include <iterator>
#include <vector>
#include <string>
#include <sstream>
#include <cstdio>
#include <fstream>
#include <filesystem>

namespace lab {

// ----------------------------------------------------------------------------
// Utilidades de archivos del worker
// ----------------------------------------------------------------------------

static std::string join_path(const std::string& a, const std::string& b) {
  return (std::filesystem::path(a) / b).string();
}

static bool write_file_text(const std::string& path, const std::string& text) {
  std::ofstream f(path, std::ios::out | std::ios::trunc | std::ios::binary);
  if (!f.is_open()) return false;
  f.write(text.data(), static_cast<std::streamsize>(text.size()));
  return f.good() || f.eof();
}

static bool read_file_text(const std::string& path, std::string& out) {
  std::ifstream f(path, std::ios::in | std::ios::binary);
  if (!f.is_open()) return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

// Une ejecutable y argumentos citando rutas con espacios. Se pasa --quiet para
// que los hijos no contaminen la salida del proceso padre.
static std::wstring build_command_line_w(const std::wstring& exe, const std::wstring& task,
                                         const std::wstring& result) {
  std::wstring cmd;
  cmd += L"\"" + exe + L"\" --worker --quiet --task \"" + task + L"\" --result \"" + result + L"\"";
  return cmd;
}

static std::wstring utf8_to_wide(const std::string& s) {
  if (s.empty()) return {};
  int size = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  if (size <= 0) return std::wstring(s.begin(), s.end());
  std::wstring out(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), size);
  return out;
}

// ----------------------------------------------------------------------------
// Un trabajo hijo: se crea primero, se espera despues.
// ----------------------------------------------------------------------------

struct child_process {
  int id = 0;
  std::string task_path;
  std::string result_path;
  PROCESS_INFORMATION pi{};
  bool created = false;
  DWORD error = 0;
};

// Crea el archivo de tarea y lanza el proceso hijo (sin esperarlo).
static void start_child(const std::wstring& exe_path, int id, uint64_t begin, uint64_t end,
                        const std::string& output_dir, child_process& child) {
  child.id = id;
  child.task_path = join_path(join_path(output_dir, "tmp"), "task_" + std::to_string(id) + ".json");
  child.result_path = join_path(join_path(output_dir, "tmp"), "result_" + std::to_string(id) + ".json");

  worker_task task;
  task.begin = begin;
  task.end = end;
  task.output_path = child.result_path;
  task.verify = true;

  if (!write_file_text(child.task_path, json::serialize_object(make_worker_task_json(task)) + "\n")) {
    child.error = ERROR_ACCESS_DENIED;
    return;
  }

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  const std::wstring cmd = build_command_line_w(
      exe_path, utf8_to_wide(child.task_path), utf8_to_wide(child.result_path));
  std::vector<wchar_t> buffer(cmd.begin(), cmd.end());
  buffer.push_back(L'\0');

  if (!CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE,
                      NORMAL_PRIORITY_CLASS, nullptr, nullptr, &si, &child.pi)) {
    child.error = GetLastError();
    return;
  }
  child.created = true;
}

// Cierra handles y opcionalmente elimina los archivos temporales.
static void finish_child(child_process& child, bool save_evidence) {
  if (child.created) {
    CloseHandle(child.pi.hThread);
    CloseHandle(child.pi.hProcess);
    child.pi = {};
  }
  if (!save_evidence) {
    std::remove(child.task_path.c_str());
    std::remove(child.result_path.c_str());
  }
}

// Lee el resultado del hijo y valida su codigo de salida.
static worker_result collect_child(child_process& child, DWORD& exit_code, bool& read_ok) {
  worker_result result;
  read_ok = false;
  exit_code = 0;
  if (!child.created) return result;

  GetExitCodeProcess(child.pi.hProcess, &exit_code);

  std::string content;
  if (!read_file_text(child.result_path, content)) return result;
  auto obj = json::parse_object_from_string(content);
  if (!obj) return result;
  result = parse_result_json(*obj);
  read_ok = true;
  return result;
}

// ----------------------------------------------------------------------------
// Experimento de procesos
// ----------------------------------------------------------------------------

std::vector<process_experiment_run> run_process_experiments(const experiment_context& ctx) {
  std::vector<process_experiment_run> runs;

  wchar_t exe_buf[MAX_PATH * 2] = {};
  GetModuleFileNameW(nullptr, exe_buf, static_cast<DWORD>(std::size(exe_buf)));
  const std::wstring exe_path(exe_buf);

  for (int workers_count : ctx.process_configs) {
    for (int repeat = 1; repeat <= ctx.repetitions; ++repeat) {
      if (ctx.cancel_flag && ctx.cancel_flag->load()) return runs;
      process_experiment_run run;
      experiment_result& r = run.result;
      r.workers = workers_count;
      r.repeat = repeat;
      r.iteration_count = static_cast<int>(ctx.total_iterations);
      r.physical_cores = ctx.hw.physical_cores;
      r.logical_processors = ctx.hw.logical_processors;
      r.experiment_name = "procesos";
      r.mode = std::to_string(workers_count) + "_procesos";

      // 1) Crear todos los hijos primero (asi se ejecutan en paralelo).
      std::vector<child_process> children(static_cast<std::size_t>(workers_count));
      measurement::timed_scope scope;
      bool launch_ok = true;
      DWORD first_error = 0;
      for (int i = 0; i < workers_count; ++i) {
        uint64_t begin = 0, end = 0;
        worker_range(ctx.total_iterations, workers_count, i, begin, end);
        start_child(exe_path, i + 1, begin, end, ctx.output_dir, children[static_cast<std::size_t>(i)]);
        if (!children[static_cast<std::size_t>(i)].created) {
          launch_ok = false;
          if (first_error == 0) first_error = children[static_cast<std::size_t>(i)].error;
        }
      }

      // 2) Esperar a todos con un unico WaitForMultipleObjects y timeout global.
      std::vector<HANDLE> handles;
      for (auto& c : children) if (c.created) handles.push_back(c.pi.hProcess);

      bool timed_out = false;
      if (!handles.empty()) {
        const DWORD timeout = ctx.timeout_ms <= 0 ? INFINITE
                              : static_cast<DWORD>(std::min<std::int64_t>(ctx.timeout_ms, 0x7FFFFFFF));
        DWORD wait = WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(),
                                            TRUE, timeout);
        if (wait == WAIT_TIMEOUT) {
          timed_out = true;
          for (HANDLE h : handles) TerminateProcess(h, EXIT_FAILURE);
        } else if (wait == WAIT_FAILED) {
          launch_ok = false;
          if (first_error == 0) first_error = GetLastError();
        }
      }

      // 3) Recolectar resultados y codigos de salida.
      uint64_t merged = 0;
      bool all_ok = true;
      std::int64_t worker_cpu_sum = 0;
      for (auto& c : children) {
        DWORD exit_code = 0;
        bool read_ok = false;
        worker_result wr = collect_child(c, exit_code, read_ok);
        if (!c.created || !read_ok || !wr.ok || exit_code != 0) all_ok = false;
        worker_cpu_sum += wr.cpu_ms;
        merged = combine_verification(merged, wr.verification_value);
        finish_child(c, ctx.save_evidence);
      }

      scope.stop();
      const measurement::timing t = scope.result();
      r.wall_ms = measurement::to_ms(t.wall);
      r.cpu_ms = measurement::to_ms(t.process_cpu);
      r.worker_cpu_ms = worker_cpu_sum;
      r.cpu_utilization_percent = measurement::cpu_utilization_percent(r.cpu_ms, r.wall_ms);
      r.peak_memory_bytes = t.peak_memory_bytes;
      r.verification_value = merged;

      const bool verification_matches =
          (ctx.baseline_verification == 0) || (merged == ctx.baseline_verification);
      r.verification_ok = launch_ok && all_ok && verification_matches && !timed_out;
      r.exit_code = r.verification_ok ? 0 : 1;
      if (!launch_ok && first_error != 0) r.win32_error = static_cast<int>(first_error);
      if (timed_out) r.win32_error = static_cast<int>(WAIT_TIMEOUT);
      r.notes = timed_out ? "timeout"
              : (r.verification_ok ? "procesos via archivos de tarea/resultado"
                                   : "fallo en lanzamiento o verificacion de procesos");

      run.launch_ok = launch_ok;
      runs.push_back(std::move(run));
    }
  }

  return runs;
}

} // namespace lab
