#pragma once
#include <string>
#include <vector>
#include "experiment_context.hpp"

namespace lab {

struct report_blocks {
  std::string objective;
  std::string assumptions;
  std::string hardware_characterization;
  std::string procedure;
  std::string configuration_table;
  std::string results;
  std::string charts;
  std::string findings;
  std::string limitations;
  std::string questions;
  std::string preliminary_conclusions;
  std::string student_fields;
};

report_blocks build_report_draft(const experiment_context& ctx, std::string measurements_csv,
                                 std::string measurements_json, std::string charts_dir);

std::string render_report_markdown(const report_blocks& blocks);

} // namespace lab
