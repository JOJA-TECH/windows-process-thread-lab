#include "terminal.hpp"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include <ctime>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace lab::terminal {

static std::string ansi(color c) {
  return "\033[" + std::to_string(static_cast<int>(c)) + "m";
}

static std::string stamp() {
  std::time_t now = std::time(nullptr);
  if (now == static_cast<std::time_t>(-1)) return "";
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &now);
#else
  localtime_r(&now, &tm);
#endif
  std::ostringstream ss;
  ss << std::put_time(&tm, "%H:%M:%S");
  return ss.str();
}

// Elimina secuencias ANSI cuando el usuario pide --no-color. Asi las funciones
// que insertan color directamente (write_section, write_kv, ...) respetan la
// opcion sin duplicar logica de color en cada una.
static std::string strip_ansi(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\033' && i + 1 < text.size() && text[i + 1] == '[') {
      i += 2;
      while (i < text.size() && text[i] != 'm') ++i;
      // el bucle for avanza i al siguiente caracter
    } else {
      out += text[i];
    }
  }
  return out;
}

static void emit(context& ctx, std::string text, color c, bool bold, bool prefix_time) {
  if (ctx.cfg.quiet) return;
  if (!ctx.cfg.colors) {
    text = strip_ansi(text);
  } else if (bold) {
    text = ansi(color::bold) + ansi(c) + text + ansi(color::reset);
  } else {
    text = ansi(c) + text + ansi(color::reset);
  }
  if (prefix_time && ctx.show_timestamps) {
    text = "[" + stamp() + "] " + text;
  }
  if (!ctx.prefix.empty()) {
    text = ctx.prefix + " " + text;
  }
  std::fputs(text.c_str(), stdout);
  std::fputc('\n', stdout);
  std::fflush(stdout);
}

void initialize(bool use_colors) {
#ifdef _WIN32
  // En Windows 10/11 la consola admite secuencias ANSI solo si se habilita
  // ENABLE_VIRTUAL_TERMINAL_PROCESSING en el handle de salida. Sin esto los
  // codigos de color apareceran como texto basura en cmd.exe clasico.
  if (!use_colors) return;
  HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
  if (h == INVALID_HANDLE_VALUE || h == nullptr) return;
  DWORD mode = 0;
  if (!GetConsoleMode(h, &mode)) return; // no es una consola (redireccion); no hay nada que hacer
  SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
  (void)use_colors;
#endif
}

bool should_show(const context& ctx) {
  return !ctx.cfg.quiet;
}

void set_title(context& ctx, std::string title) {
  ctx.title = std::move(title);
}

void set_prefix(context& ctx, std::string prefix) {
  ctx.prefix = std::move(prefix);
}

void write_line(context& ctx, std::string text, color c, bool bold) {
  emit(ctx, text, c, bold, false);
}

void write_section(context& ctx, std::string header) {
  std::string line;
  line += ansi(color::bold_blue);
  line += "≫ ";
  line += header;
  line += ansi(color::reset);
  emit(ctx, line, color::reset, true, false);
}

void write_kv(context& ctx, std::string key, std::string value, color val_color) {
  std::string line;
  line += ansi(color::bold_blue);
  line += key;
  line += ": ";
  line += ansi(val_color);
  line += value;
  line += ansi(color::reset);
  emit(ctx, line, color::reset, false, false);
}

void write_status(context& ctx, std::string label, std::string status) {
  std::string line;
  line += ansi(color::bold);
  line += label;
  line += " → ";
  line += ansi(color::bold_green);
  line += status;
  line += ansi(color::reset);
  emit(ctx, line, color::reset, false, false);
}

void write_summary(context& ctx, std::vector<std::string> lines) {
  write_section(ctx, "Resumen");
  for (auto& ln : lines) {
    write_line(ctx, ln);
  }
}

void write_progress(context& ctx, std::string stage, std::size_t current, std::size_t total) {
  if (!ctx.cfg.progress_inline) {
    write_line(ctx, stage + " (" + std::to_string(current) + "/" + std::to_string(total) + ")", color::cyan);
    return;
  }
  std::string line;
  line += "\r";
  line += ansi(color::cyan);
  line += "  " + stage;
  line += " [" + std::to_string(current) + "/" + std::to_string(total) + "]";
  line += ansi(color::reset);
  // borrar hasta final de línea para reutilizar la línea
  std::fputs(line.c_str(), stdout);
  std::fflush(stdout);
}

void write_progress_done(context& ctx, std::string stage) {
  if (!ctx.cfg.progress_inline) {
    write_line(ctx, stage + " completado", color::green);
    return;
  }
  std::fputc('\n', stdout);
  std::fflush(stdout);
  write_status(ctx, stage, "completado");
}

void write_quoted(context& ctx, std::string text, color c) {
  std::string line;
  line += ansi(c);
  line += "\"" + text + "\"";
  line += ansi(color::reset);
  emit(ctx, line, color::reset, false, false);
}

void write_error(context& ctx, std::string text) {
  emit(ctx, "✖ " + text, color::bold_red, true, true);
}
void write_warning(context& ctx, std::string text) {
  emit(ctx, "⚠ " + text, color::yellow, false, true);
}
void write_info(context& ctx, std::string text) {
  emit(ctx, "ℹ " + text, color::blue, false, true);
}
void write_ok(context& ctx, std::string text) {
  emit(ctx, "✔ " + text, color::bold_green, false, true);
}
void write_muted(context& ctx, std::string text) {
  emit(ctx, "· " + text, color::dim, false, false);
}

std::string escape_csv_value(std::string text) {
  bool needs_quoting = false;
  for (auto ch : text) {
    if (ch == ',' || ch == '"' || ch == '\n' || ch == '\r') {
      needs_quoting = true;
      break;
    }
  }
  if (!needs_quoting) return text;
  std::string out = "\"";
  for (auto ch : text) {
    if (ch == '"') out += "\"\"";
    else out += ch;
  }
  out += "\"";
  return out;
}

std::string escape_json_string(std::string text) {
  std::string out;
  out.reserve(text.size() + 16);
  for (auto ch : text) {
    switch (ch) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) {
          out += "\\u00" + std::to_string(static_cast<unsigned int>(static_cast<unsigned char>(ch)));
        } else {
          out += ch;
        }
    }
  }
  return out;
}

} // namespace lab::terminal
