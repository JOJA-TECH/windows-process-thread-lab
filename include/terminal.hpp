#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace lab::terminal {

enum class color : int {
  reset = 0,
  bold = 1,
  dim = 2,
  red = 31,
  green = 32,
  yellow = 33,
  blue = 34,
  magenta = 35,
  cyan = 36,
  bold_red = 91,
  bold_green = 92,
  bold_blue = 94,
};

struct config {
  bool quiet = false;
  bool colors = true;
  bool progress_inline = true;
};

struct context {
  config cfg;
  std::string title;
  std::string prefix;
  bool show_timestamps = false;
};

void initialize(bool use_colors);
bool should_show(const context& ctx);
void set_title(context& ctx, std::string title);
void set_prefix(context& ctx, std::string prefix);
void write_line(context& ctx, std::string text, color c = color::reset, bool bold = false);
void write_section(context& ctx, std::string header);
void write_kv(context& ctx, std::string key, std::string value, color val_color = color::reset);
void write_status(context& ctx, std::string label, std::string status);
void write_summary(context& ctx, std::vector<std::string> lines);
void write_progress(context& ctx, std::string stage, std::size_t current, std::size_t total);
void write_progress_done(context& ctx, std::string stage);
void write_quoted(context& ctx, std::string text, color c = color::reset);
void write_error(context& ctx, std::string text);
void write_warning(context& ctx, std::string text);
void write_info(context& ctx, std::string text);
void write_ok(context& ctx, std::string text);
void write_muted(context& ctx, std::string text);

std::string escape_csv_value(std::string text);
std::string escape_json_string(std::string text);

} // namespace lab::terminal
