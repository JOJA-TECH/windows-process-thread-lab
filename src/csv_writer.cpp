#include "csv_writer.hpp"
#include <stdexcept>
#include <ios>

namespace lab::csv {

std::string escape_csv_field(std::string text) {
  bool needs_quotes = false;
  for (char c : text) {
    if (c == ',' || c == '"' || c == '\n' || c == '\r') {
      needs_quotes = true;
      break;
    }
  }
  if (!needs_quotes) return text;
  std::string out = "\"";
  for (char c : text) {
    if (c == '"') out += "\"\"";
    else out += c;
  }
  out += "\"";
  return out;
}

std::vector<std::string> split_header(std::string_view header_text) {
  std::vector<std::string> out;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= header_text.size(); ++i) {
    if (i == header_text.size() || header_text[i] == ',') {
      out.emplace_back(header_text.substr(start, i - start));
      start = i + 1;
    }
  }
  return out;
}

writer::writer(std::string path) : path_(std::move(path)) {
  file_.open(path_, std::ios::out | std::ios::trunc);
  if (!file_.is_open()) {
    throw std::runtime_error("No se pudo abrir el archivo CSV: " + path_);
  }
  // Normalmente el sistema operativo maneja la codificación; para este laboratorio
  // se usa UTF-8 y se evita reinterpretar bytes.
}

writer::~writer() {
  if (file_.is_open()) {
    file_.close();
  }
}

void writer::set_header(std::vector<std::string> header) {
  header_ = std::move(header);
}

void writer::write_record(record r) {
  if (!wrote_header_ && !header_.empty()) {
    std::string line;
    for (std::size_t i = 0; i < header_.size(); ++i) {
      if (i) line += ',';
      line += escape_csv_field(header_[i]);
    }
    file_ << line << '\n';
    wrote_header_ = true;
  }

  if (r.fields.empty()) return;
  std::string line;
  for (std::size_t i = 0; i < r.fields.size(); ++i) {
    if (i) line += ',';
    line += escape_csv_field(r.fields[i]);
  }
  file_ << line << '\n';
}

void writer::finish() {
  if (file_.is_open()) {
    file_.flush();
  }
}

} // namespace lab::csv
