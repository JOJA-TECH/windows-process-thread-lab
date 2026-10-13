#pragma once
#include <vector>
#include <optional>
#include "experiment_context.hpp"

namespace lab {

struct process_experiment_run {
  experiment_result result;
  bool launch_ok = true;
  std::optional<int> win32_error;
};

std::vector<process_experiment_run> run_process_experiments(const experiment_context& ctx);

} // namespace lab
