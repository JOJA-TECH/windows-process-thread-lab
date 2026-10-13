#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <variant>
#include <optional>

namespace lab::json {

// JSON minimo y seguro para los usos del laboratorio.
// No intenta ser un parser general; se usa para escritura estructurada
// (hardware.json, measurements.json) y para lectura ligera de las tareas
// del worker. Se eligio un objeto liviano en lugar de una libreria externa
// para que el laboratorio no dependa de Internet ni de paquetes adicionales.

struct value;
using object = std::map<std::string, value>; // orden alfabetico estable
using array = std::vector<value>;

struct value {
  enum class kind {
    null,
    boolean,
    integer,
    number,
    string,
    object,
    array,
  };

  kind type() const { return kind_; }

  // Constructor por defecto requerido por std::map<std::string, value>.
  value() : kind_{kind::null}, storage_{std::nullptr_t{}} {}

  // Constructores de fabrica. Se usan en lugar de exponer el constructor.
  static value null_value() { return value{kind::null, std::nullptr_t{}}; }
  static value from_bool(bool b) { return value{kind::boolean, b}; }
  static value from_int(int64_t v) { return value{kind::integer, v}; }
  static value from_double(double v) { return value{kind::number, v}; }
  static value from_string(std::string v) { return value{kind::string, std::move(v)}; }
  static value from_object(object v) { return value{kind::object, std::move(v)}; }
  static value from_array(array v) { return value{kind::array, std::move(v)}; }

  // Accesores tolerantes a tipo erroneo: devuelven un valor por defecto en vez
  // de lanzar, para no romper al leer archivos parcialmente escritos.
  bool is_null() const { return kind_ == kind::null; }
  bool as_bool(bool def = false) const {
    const bool* p = std::get_if<bool>(&storage_);
    return p ? *p : def;
  }
  int64_t as_int(int64_t def = 0) const {
    const int64_t* p = std::get_if<int64_t>(&storage_);
    return p ? *p : def;
  }
  double as_double(double def = 0.0) const {
    const double* p = std::get_if<double>(&storage_);
    if (p) return *p;
    const int64_t* i = std::get_if<int64_t>(&storage_);
    return i ? static_cast<double>(*i) : def;
  }
  const std::string& as_string() const {
    static const std::string empty;
    const std::string* p = std::get_if<std::string>(&storage_);
    return p ? *p : empty;
  }
  const object& as_object() const {
    static const object empty;
    const object* p = std::get_if<object>(&storage_);
    return p ? *p : empty;
  }
  const array& as_array() const {
    static const array empty;
    const array* p = std::get_if<array>(&storage_);
    return p ? *p : empty;
  }

  // Devuelve una copia del valor asociado a key si este value es un objeto.
  std::optional<value> get(const std::string& key) const {
    const object* o = std::get_if<object>(&storage_);
    if (!o) return std::nullopt;
    auto it = o->find(key);
    if (it == o->end()) return std::nullopt;
    return it->second;
  }

private:
  value(kind k, std::nullptr_t) : kind_{k} {}
  value(kind k, bool b) : kind_{k}, storage_{b} {}
  value(kind k, int64_t v) : kind_{k}, storage_{v} {}
  value(kind k, double v) : kind_{k}, storage_{v} {}
  value(kind k, std::string v) : kind_{k}, storage_{std::move(v)} {}
  value(kind k, object v) : kind_{k}, storage_{std::move(v)} {}
  value(kind k, array v) : kind_{k}, storage_{std::move(v)} {}

  kind kind_;
  std::variant<std::nullptr_t, bool, int64_t, double, std::string, object, array> storage_;
};

// Busca una clave en un objeto sin copiarlo. Devuelve nullptr si no existe.
// Util para leer tareas/resultados del worker sin construir un value temporal.
inline const value* find(const object& o, const std::string& key) {
  auto it = o.find(key);
  return it == o.end() ? nullptr : &it->second;
}

std::string serialize(const value& v, int indent = 2, int depth = 0);
std::string serialize_compact(const value& v);

// Atajo para serializar directamente un objeto como raiz JSON.
inline std::string serialize_object(const object& o, int indent = 2) {
  return serialize(value::from_object(o), indent, 0);
}

// Lectura minima para el worker. No es un parser general, pero cubre el
// formato que el laboratorio genera y lee.
std::optional<object> parse_object_from_string(std::string_view text);

} // namespace lab::json
