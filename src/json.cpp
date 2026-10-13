#include "json.hpp"
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>

namespace lab::json {

static std::string indent_str(int depth, int indent) {
  return std::string(depth * indent, ' ');
}

static std::string to_json_string(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 16);
  out.push_back('"');
  for (char c : s) {
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          out += "\\u00";
          out += "0123456789abcdef"[c >> 4];
          out += "0123456789abcdef"[c & 0xF];
        } else {
          out.push_back(c);
        }
    }
  }
  out.push_back('"');
  return out;
}

static std::string number_to_string(double v) {
  if (std::isfinite(v)) {
    std::ostringstream ss;
    ss << std::setprecision(12) << v;
    std::string s = ss.str();
    // Normalizar la escritura de números enteros que se expresaron como double
    // pero que no tienen parte fraccionaria en la precisión usada.
    if (std::floor(v) == v) {
      std::ostringstream ss2;
      ss2 << std::setprecision(12) << std::fixed << v;
      std::string s2 = ss2.str();
      // Eliminar ceros trailing después del punto decimal para enteros
      auto dot = s2.find('.');
      if (dot != std::string::npos) {
        auto end = s2.find_last_not_of('0');
        if (end == dot) end = dot - 1;
        s = s2.substr(0, end + 1);
        if (!s.empty() && s.back() == '.') s.pop_back();
      }
    }
    return s;
  }
  if (std::isinf(v)) return v > 0 ? "Infinity" : "-Infinity";
  if (std::isnan(v)) return "NaN";
  return "null";
}

static std::string serialize_value(const value& v, int indent, int depth, bool compact) {
  switch (v.type()) {
    case value::kind::null:
      return "null";
    case value::kind::boolean:
      return v.as_bool() ? "true" : "false";
    case value::kind::integer:
      return std::to_string(v.as_int());
    case value::kind::number:
      return number_to_string(v.as_double());
    case value::kind::string:
      return to_json_string(v.as_string());
    case value::kind::array: {
      const auto& arr = v.as_array();
      if (arr.empty()) return "[]";
      if (compact) {
        std::string out = "[";
        for (std::size_t i = 0; i < arr.size(); ++i) {
          if (i) out += ",";
          out += serialize_value(arr[i], indent, depth, true);
        }
        out += "]";
        return out;
      }
      std::string out = "[\n";
      for (std::size_t i = 0; i < arr.size(); ++i) {
        if (i) out += ",\n";
        out += indent_str(depth + 1, indent);
        out += serialize_value(arr[i], indent, depth + 1, false);
      }
      out += "\n" + indent_str(depth, indent) + "]";
      return out;
    }
    case value::kind::object: {
      const auto& obj = v.as_object();
      if (obj.empty()) return "{}";
      if (compact) {
        std::string out = "{";
        bool first = true;
        for (auto& [k, val] : obj) {
          if (!first) out += ",";
          first = false;
          out += to_json_string(k) + ":" + serialize_value(val, indent, depth, true);
        }
        out += "}";
        return out;
      }
      std::string out = "{\n";
      bool first = true;
      for (auto& [k, val] : obj) {
        if (!first) out += ",\n";
        first = false;
        out += indent_str(depth + 1, indent);
        out += to_json_string(k) + ": " + serialize_value(val, indent, depth + 1, false);
      }
      out += "\n" + indent_str(depth, indent) + "}";
      return out;
    }
  }
  return "null";
}

std::string serialize(const value& v, int indent, int depth) {
  return serialize_value(v, indent, depth, false);
}

std::string serialize_compact(const value& v) {
  return serialize_value(v, 0, 0, true);
}

// ----------------------------------------------------------------------------
// Parser ligero de objetos JSON
// ----------------------------------------------------------------------------

namespace {

struct parser {
  std::string_view input;
  std::size_t pos = 0;

  void skip_ws() {
    while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
      ++pos;
    }
  }

  char peek() {
    skip_ws();
    if (pos >= input.size()) return '\0';
    return input[pos];
  }

  char next() {
    skip_ws();
    if (pos >= input.size()) return '\0';
    return input[pos++];
  }

  void expect(char c) {
    if (next() != c) {
      throw std::runtime_error(std::string("JSON parse error: expected '") + c + "'");
    }
  }

  std::string parse_string() {
    expect('"');
    std::string out;
    while (pos < input.size() && input[pos] != '"') {
      if (input[pos] == '\\') {
        ++pos;
        if (pos >= input.size()) throw std::runtime_error("JSON parse error: unexpected end in string");
        switch (input[pos]) {
          case '"': out.push_back('"'); break;
          case '\\': out.push_back('\\'); break;
          case '/': out.push_back('/'); break;
          case 'b': out.push_back('\b'); break;
          case 'f': out.push_back('\f'); break;
          case 'n': out.push_back('\n'); break;
          case 'r': out.push_back('\r'); break;
          case 't': out.push_back('\t'); break;
          case 'u': {
            ++pos;
            if (pos + 4 > input.size()) throw std::runtime_error("JSON parse error: invalid \\uXXXX");
            std::string hex(input.substr(pos, 4));
            pos += 4;
            unsigned int cp = 0;
            for (char h : hex) {
              cp <<= 4;
              if (h >= '0' && h <= '9') cp |= (h - '0');
              else if (h >= 'a' && h <= 'f') cp |= (10 + h - 'a');
              else if (h >= 'A' && h <= 'F') cp |= (10 + h - 'A');
              else throw std::runtime_error("JSON parse error: invalid \\u hex");
            }
            if (cp <= 0x7F) {
              out.push_back(static_cast<char>(cp));
            } else if (cp <= 0x7FF) {
              out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
              out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else {
              // UTF-8 simple para BMP; empezamos asumiendo sustitución si fuera suplementario
              out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
              out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
              out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            continue;
          }
          default:
            throw std::runtime_error("JSON parse error: invalid escape");
        }
      } else {
        out.push_back(input[pos]);
      }
      ++pos;
    }
    if (pos >= input.size()) throw std::runtime_error("JSON parse error: unterminated string");
    ++pos; // skip closing quote
    return out;
  }

  value parse_value() {
    char c = peek();
    if (c == '"') return value::from_string(parse_string());
    if (c == '{') {
      ++pos; // skip '{'
      object obj;
      skip_ws();
      if (pos < input.size() && input[pos] == '}') {
        ++pos;
        return value::from_object(std::move(obj));
      }
      while (true) {
        skip_ws();
        std::string key = parse_string();
        skip_ws();
        expect(':');
        obj.emplace(key, parse_value());
        skip_ws();
        if (pos < input.size() && input[pos] == ',') {
          ++pos;
          continue;
        }
        break;
      }
      skip_ws();
      expect('}');
      return value::from_object(std::move(obj));
    }
    if (c == '[') {
      ++pos;
      array arr;
      skip_ws();
      if (pos < input.size() && input[pos] == ']') {
        ++pos;
        return value::from_array(std::move(arr));
      }
      while (true) {
        arr.push_back(parse_value());
        skip_ws();
        if (pos < input.size() && input[pos] == ',') {
          ++pos;
          continue;
        }
        break;
      }
      skip_ws();
      expect(']');
      return value::from_array(std::move(arr));
    }
    if (c == 't') {
      if (input.substr(pos, 4) == "true") { pos += 4; return value::from_bool(true); }
      throw std::runtime_error("JSON parse error: unexpected token");
    }
    if (c == 'f') {
      if (input.substr(pos, 5) == "false") { pos += 5; return value::from_bool(false); }
      throw std::runtime_error("JSON parse error: unexpected token");
    }
    if (c == 'n') {
      if (input.substr(pos, 4) == "null") { pos += 4; return value::null_value(); }
      throw std::runtime_error("JSON parse error: unexpected token");
    }
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
      return parse_number();
    }
    throw std::runtime_error("JSON parse error: unexpected character");
  }

  value parse_number() {
    skip_ws();
    std::size_t start = pos;
    if (pos < input.size() && input[pos] == '-') ++pos;
    if (pos >= input.size() || !std::isdigit(static_cast<unsigned char>(input[pos])))
      throw std::runtime_error("JSON parse error: invalid number");
    while (pos < input.size() && std::isdigit(static_cast<unsigned char>(input[pos]))) ++pos;
    bool is_float = false;
    if (pos < input.size() && input[pos] == '.') {
      is_float = true;
      ++pos;
      if (pos >= input.size() || !std::isdigit(static_cast<unsigned char>(input[pos])))
        throw std::runtime_error("JSON parse error: invalid number");
      while (pos < input.size() && std::isdigit(static_cast<unsigned char>(input[pos]))) ++pos;
    }
    if (pos < input.size() && (input[pos] == 'e' || input[pos] == 'E')) {
      is_float = true;
      ++pos;
      if (pos < input.size() && (input[pos] == '+' || input[pos] == '-')) ++pos;
      if (pos >= input.size() || !std::isdigit(static_cast<unsigned char>(input[pos])))
        throw std::runtime_error("JSON parse error: invalid number");
      while (pos < input.size() && std::isdigit(static_cast<unsigned char>(input[pos]))) ++pos;
    }
    std::string num(input.substr(start, pos - start));
    if (is_float) {
      try {
        return value::from_double(std::stod(num));
      } catch (...) {
        throw std::runtime_error("JSON parse error: invalid number");
      }
    } else {
      try {
        return value::from_int(static_cast<int64_t>(std::stoll(num)));
      } catch (...) {
        throw std::runtime_error("JSON parse error: invalid number");
      }
    }
  }
};

} // namespace

std::optional<object> parse_object_from_string(std::string_view text) {
  try {
    parser p{text};
    p.skip_ws();
    value v = p.parse_value();
    p.skip_ws();
    if (p.pos != text.size()) {
      // Permite espacios finales, pero no tokens extra
      for (auto i = p.pos; i < text.size(); ++i) {
        if (!std::isspace(static_cast<unsigned char>(text[i]))) {
          return std::nullopt;
        }
      }
    }
    if (v.type() != value::kind::object) return std::nullopt;
    return v.as_object();
  } catch (...) {
    return std::nullopt;
  }
}

} // namespace lab::json
