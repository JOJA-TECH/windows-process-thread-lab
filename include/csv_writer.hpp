#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <string_view>
#include <fstream>

namespace lab::csv {

struct record {
  std::vector<std::string> fields;
};

class writer {
public:
  explicit writer(std::string path);
  ~writer();

  void set_header(std::vector<std::string> header);
  void write_record(record r);
  void finish();

  std::string path() const { return path_; }

private:
  std::ofstream file_;
  std::string path_;
  std::vector<std::string> header_;
  bool wrote_header_ = false;
};

std::string escape_csv_field(std::string text);
std::vector<std::string> split_header(std::string_view header_text);

} // namespace lab::csv
