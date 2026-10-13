#pragma once
#include <string>
#include <cstdint>
#include <optional>
#include <vector>

namespace lab {

struct program_options {
  bool help = false;
  bool worker = false;
  std::string worker_task_path;
  std::string worker_result_path;

  std::string output_dir = "results";
  uint64_t iterations = 50'000'000;
  int repetitions = 3;
  int max_workers = -1; // -1 = automatico
  int max_processes = -1; // -1 = automatico
  int timeout_seconds = 120;
  bool skip_processes = false;
  bool skip_affinity = false;
  bool quick = false;
  bool no_color = false;
  bool quiet = false;
  bool save_evidence = false;

  std::optional<std::string> affinity_cpu; // no usado directamente aqui, solo documentado

  std::vector<std::string> positional;
};

program_options parse_args(int argc, char** argv);
std::string usage_text();

} // namespace lab
