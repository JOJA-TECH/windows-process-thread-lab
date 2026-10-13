#include "hardware_info.hpp"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 // GetLogicalProcessorInformationEx (Windows 7+)
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <sysinfoapi.h>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <vector>

namespace lab {

// ----------------------------------------------------------------------------
// Utilidades internas
// ----------------------------------------------------------------------------

static std::string now_timestamp() {
  std::time_t t = std::time(nullptr);
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  std::ostringstream ss;
  ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

static std::string compiler_string() {
#ifdef _MSC_VER
  std::ostringstream ss;
  ss << "MSVC " << _MSC_VER;
#ifdef _MSVC_LANG
  ss << " (C++" << _MSVC_LANG << ")";
#endif
  return ss.str();
#elif defined(__GNUC__)
  std::ostringstream ss;
  ss << "GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__;
#ifdef __MINGW32__
  ss << " (MinGW-w64)";
#endif
  ss << " (C++" << __cplusplus << ")";
  return ss.str();
#else
  return "compilador desconocido";
#endif
}

// Convierte UTF-16 a UTF-8 conservando caracteres no ASCII (nombres de CPU,
// rutas con acentos, etc.), evitando el reemplazo por '?'.
static std::string wide_to_utf8(const std::wstring& ws) {
  if (ws.empty()) return {};
#ifdef _WIN32
  int size = WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()),
                                 nullptr, 0, nullptr, nullptr);
  if (size <= 0) return {};
  std::string out(static_cast<std::size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()),
                      out.data(), size, nullptr, nullptr);
  return out;
#else
  return std::string(ws.begin(), ws.end());
#endif
}

static std::string architecture_name(WORD arch) {
  switch (arch) {
    case PROCESSOR_ARCHITECTURE_AMD64: return "x86_64";
    case PROCESSOR_ARCHITECTURE_ARM:   return "arm";
    case PROCESSOR_ARCHITECTURE_ARM64: return "arm64";
    case PROCESSOR_ARCHITECTURE_IA64:  return "ia64";
    case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
    default:                           return "desconocida";
  }
}

static bool reg_read_string(HKEY root, const wchar_t* subkey, const wchar_t* name, std::wstring& out) {
  HKEY hkey = nullptr;
  LONG status = RegOpenKeyExW(root, subkey, 0, KEY_READ, &hkey);
  if (status != ERROR_SUCCESS) return false;

  wchar_t buffer[512] = {};
  DWORD type = 0;
  DWORD size = sizeof(buffer);
  status = RegQueryValueExW(hkey, name, nullptr, &type, reinterpret_cast<BYTE*>(buffer), &size);
  RegCloseKey(hkey);
  if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return false;

  out.assign(buffer);
  return true;
}

static std::string cpu_name_from_registry() {
  std::wstring value;
  if (reg_read_string(HKEY_LOCAL_MACHINE,
                      L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      L"ProcessorNameString", value)) {
    std::string name = wide_to_utf8(value);
    // Limpia espacios redundantes que suele dejar el registro.
    while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) name.pop_back();
    if (!name.empty()) return name;
  }
  return "CPU (nombre no detectado)";
}

// El registro es mas fiable que GetVersionEx en Windows 10/11. El build 22000+
// corresponde a Windows 11 (GetVersionEx sin manifiesto puede reportar 6.2).
static std::string os_name_from_registry() {
  const wchar_t* key = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
  std::wstring product, build, display;
  bool has_product = reg_read_string(HKEY_LOCAL_MACHINE, key, L"ProductName", product);
  bool has_build = reg_read_string(HKEY_LOCAL_MACHINE, key, L"CurrentBuild", build);
  reg_read_string(HKEY_LOCAL_MACHINE, key, L"DisplayVersion", display);

  if (!has_product && !has_build) return "Windows (version no disponible)";

  std::string name = wide_to_utf8(product);
  int build_num = has_build ? std::atoi(wide_to_utf8(build).c_str()) : 0;
  if (build_num >= 22000 && name.find("Windows 10") != std::string::npos) {
    // Windows 11 conserva "Windows 10" en ProductName en muchas instalaciones.
    std::string::size_type pos = name.find("Windows 10");
    name.replace(pos, std::string("Windows 10").size(), "Windows 11");
  }
  if (!display.empty()) name += " (build " + std::to_string(build_num) + ", " + wide_to_utf8(display) + ")";
  return name;
}

// ----------------------------------------------------------------------------
// Deteccion de hardware
// ----------------------------------------------------------------------------

// Definida mas abajo. Rellena recommended_*_configs y las razones.
void compute_config_plan(hardware_info& hw, int lp_limit, int process_limit);

hardware_info detect_hardware() {
  hardware_info hw;
  hw.timestamp = now_timestamp();
  hw.cpu_name = cpu_name_from_registry();
  hw.compiler_id = compiler_string();

  // Arquitectura del proceso (GetSystemInfo) y nativa del sistema (GetNativeSystemInfo).
  SYSTEM_INFO si{};
  GetSystemInfo(&si);
  hw.arch_process = architecture_name(si.wProcessorArchitecture);
  SYSTEM_INFO nsi{};
  GetNativeSystemInfo(&nsi);
  hw.arch_system = architecture_name(nsi.wProcessorArchitecture);

  hw.os_name = os_name_from_registry();

  // Procesadores logicos activos de todos los grupos (API requerida por AGENT.md).
  DWORD lp_all = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
  if (lp_all == 0) {
    // Respaldo si la API de grupos no esta disponible.
    hw.logical_processors = static_cast<int>(si.dwNumberOfProcessors);
  } else {
    hw.logical_processors = static_cast<int>(lp_all);
  }
  hw.processor_groups = static_cast<int>(GetActiveProcessorGroupCount());

  // Memoria fisica total y disponible.
  MEMORYSTATUSEX mem{};
  mem.dwLength = sizeof(mem);
  if (GlobalMemoryStatusEx(&mem)) {
    hw.total_memory_bytes = static_cast<std::int64_t>(mem.ullTotalPhys);
    hw.available_memory_bytes = static_cast<std::int64_t>(mem.ullAvailPhys);
  }

  // Base de cada grupo de procesadores, necesaria para traducir mascaras de
  // grupo a indices globales de procesador logico.
  const int group_count = std::max(1, hw.processor_groups);
  std::vector<int> group_base(static_cast<std::size_t>(group_count), 0);
  for (int g = 1; g < group_count; ++g) {
    group_base[static_cast<std::size_t>(g)] =
        group_base[static_cast<std::size_t>(g - 1)] + static_cast<int>(GetActiveProcessorCount(static_cast<WORD>(g)));
  }

  // Nucleos fisicos y su relacion con los procesadores logicos.
  DWORD len = 0;
  if (!GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &len) &&
      GetLastError() == ERROR_INSUFFICIENT_BUFFER && len > 0) {
    std::vector<unsigned char> buffer(len);
    if (GetLogicalProcessorInformationEx(
            RelationProcessorCore,
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()), &len)) {
      unsigned char* p = buffer.data();
      unsigned char* end = buffer.data() + len;
      int core_index = 0;
      while (p < end) {
        auto* entry = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(p);
        if (entry->Relationship == RelationProcessorCore) {
          int thread_count = 0;
          int first_logical = -1;
          for (WORD g = 0; g < entry->Processor.GroupCount; ++g) {
            const ULONG_PTR mask = entry->Processor.GroupMask[g].Mask;
            const int base = group_base[static_cast<std::size_t>(g)];
            for (int i = 0; i < 64; ++i) {
              if (mask & (static_cast<ULONG_PTR>(1) << i)) {
                ++thread_count;
                if (first_logical < 0) first_logical = base + i;
              }
            }
          }
          if (first_logical >= 0) {
            hw.core_layout.push_back({first_logical, core_index, thread_count > 1});
          }
          ++core_index;
        }
        p += entry->Size;
      }
      hw.physical_cores = core_index; // 0 => NA
    }
  }

  // Sockets/packages fisicos habilitados.
  DWORD pkg_len = 0;
  if (!GetLogicalProcessorInformationEx(RelationProcessorPackage, nullptr, &pkg_len) &&
      GetLastError() == ERROR_INSUFFICIENT_BUFFER && pkg_len > 0) {
    std::vector<unsigned char> buffer(pkg_len);
    if (GetLogicalProcessorInformationEx(
            RelationProcessorPackage,
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()), &pkg_len)) {
      unsigned char* p = buffer.data();
      unsigned char* end = buffer.data() + pkg_len;
      int packages = 0;
      while (p < end) {
        auto* entry = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(p);
        if (entry->Relationship == RelationProcessorPackage) ++packages;
        p += entry->Size;
      }
      hw.physical_cpus = packages;
    }
  }

  const int lp_limit = std::min(hw.logical_processors, 32);
  const int process_limit = std::min(hw.logical_processors, 16);
  compute_config_plan(hw, lp_limit, process_limit);
  hw.configuration_rationale = build_configuration_rationale(hw, lp_limit, process_limit);
  hw.limitations = build_limitations_text(hw, lp_limit, process_limit);

  return hw;
}

// Rellena las configuraciones recomendadas y la razon de inclusion/exclusion de
// cada una. Se separa de detect_hardware para mantener el codigo legible.
void compute_config_plan(hardware_info& hw, int lp_limit, int process_limit) {
  static const int base_set[] = {1, 2, 4, 8, 16, 32};

  // Hilos: potencias de 2 <= LP, siempre 1, incluir LP exacto y sobre-suscripcion.
  std::vector<int> threads;
  std::vector<std::string> threads_reason;
  threads.push_back(1);
  threads_reason.push_back("1 hilo: linea base de paralelismo, siempre incluida.");

  for (int b : base_set) {
    if (b == 1) continue;
    if (b <= lp_limit) {
      threads.push_back(b);
      threads_reason.push_back(std::to_string(b) + " hilos: potencia de 2 <= LP (" +
                               std::to_string(lp_limit) + ").");
    } else {
      threads_reason.push_back(std::to_string(b) + " hilos: EXCLUIDA por exceder LP=" +
                               std::to_string(lp_limit) + ".");
    }
  }
  if (lp_limit > 1 && std::find(threads.begin(), threads.end(), lp_limit) == threads.end()) {
    threads.push_back(lp_limit);
    threads_reason.push_back(std::to_string(lp_limit) + " hilos: LP exacto aunque no sea potencia de 2.");
  }
  if (lp_limit < 32) {
    const int over = lp_limit + 1;
    if (over <= 32) {
      threads.push_back(over);
      threads_reason.push_back(std::to_string(over) + " hilos: sobre-suscripcion (LP + 1), LP < 32.");
    }
  } else {
    threads_reason.push_back("Sobre-suscripcion (LP + 1): EXCLUIDA porque LP >= 32.");
  }
  if (hw.logical_processors == 1) {
    threads_reason.push_back("LP == 1: solo se ejecuta 1 hilo; no se puede medir paralelismo fisico.");
  }

  std::sort(threads.begin(), threads.end());
  threads.erase(std::unique(threads.begin(), threads.end()), threads.end());
  hw.recommended_thread_configs = threads;
  hw.thread_config_reasons = threads_reason;

  // Procesos: misma lista pero limitada a min(LP, 16).
  std::vector<int> processes;
  std::vector<std::string> processes_reason;
  processes.push_back(1);
  processes_reason.push_back("1 proceso: linea base, siempre incluida.");
  for (int b : base_set) {
    if (b == 1) continue;
    if (b <= process_limit) {
      processes.push_back(b);
      processes_reason.push_back(std::to_string(b) + " procesos: potencia de 2 <= min(LP, 16) (" +
                                 std::to_string(process_limit) + ").");
    } else {
      processes_reason.push_back(std::to_string(b) + " procesos: EXCLUIDA por exceder el limite de procesos=" +
                                 std::to_string(process_limit) + ".");
    }
  }
  if (process_limit > 1 && std::find(processes.begin(), processes.end(), process_limit) == processes.end()) {
    processes.push_back(process_limit);
    processes_reason.push_back(std::to_string(process_limit) + " procesos: limite exacto min(LP, 16).");
  }
  std::sort(processes.begin(), processes.end());
  processes.erase(std::unique(processes.begin(), processes.end()), processes.end());
  hw.recommended_process_configs = processes;
  hw.process_config_reasons = processes_reason;

  // Si la memoria disponible es baja, reducir procesos y dejarlo como limitacion.
  if (hw.available_memory_bytes > 0 && hw.available_memory_bytes < (1LL << 30)) {
    const int max_by_mem = 2;
    std::vector<std::string> reduced;
    for (const std::string& r : processes_reason) reduced.push_back(r);
    processes.erase(std::remove_if(processes.begin(), processes.end(),
                                   [&](int w) { return w > max_by_mem; }),
                    processes.end());
    reduced.push_back("Memoria disponible < 1 GB: procesos reducidos a <= " +
                      std::to_string(max_by_mem) + ".");
    hw.recommended_process_configs = processes;
    hw.process_config_reasons = reduced;
  }
}

std::string build_configuration_rationale(const hardware_info& hw, int lp_limit, int process_limit) {
  std::ostringstream ss;
  ss << "LP = " << hw.logical_processors << " procesadores logicos activos; "
     << "C = " << hw.physical_cores << " nucleos fisicos detectados (NA si 0); "
     << "LP limit = " << lp_limit << "; proceso limit = " << process_limit << ".\n";

  ss << "Hilos: ";
  for (std::size_t i = 0; i < hw.recommended_thread_configs.size(); ++i) {
    if (i) ss << ", ";
    ss << hw.recommended_thread_configs[i];
  }
  ss << ".\n";
  for (const std::string& r : hw.thread_config_reasons) ss << "  - " << r << "\n";

  ss << "Procesos: ";
  for (std::size_t i = 0; i < hw.recommended_process_configs.size(); ++i) {
    if (i) ss << ", ";
    ss << hw.recommended_process_configs[i];
  }
  ss << ".\n";
  for (const std::string& r : hw.process_config_reasons) ss << "  - " << r << "\n";

  return ss.str();
}

std::string build_limitations_text(const hardware_info& hw, int lp_limit, int process_limit) {
  std::ostringstream ss;
  if (hw.logical_processors <= 1) {
    ss << "LP <= 1: no es posible medir paralelismo fisico real. ";
  }
  if (hw.physical_cores == 0) {
    ss << "Nucleos fisicos no detectados (NA). ";
  }
  if (hw.physical_cpus == 0) {
    ss << "Sockets fisicos no detectados (NA). ";
  }
  if (hw.available_memory_bytes > 0 && hw.available_memory_bytes < (1LL << 30)) {
    ss << "Memoria disponible baja (< 1 GB): posible limitacion para procesos. ";
  }
  if (hw.processor_groups > 1) {
    ss << "Multiples grupos de procesadores detectados; la afinidad puede ser limitada. ";
  }
  ss << "Limites de ejecucion automatica: max 32 hilos; max 16 procesos; "
     << "timeout configurable por prueba.";
  (void)lp_limit;
  (void)process_limit;
  return ss.str();
}

// ----------------------------------------------------------------------------
// Serializacion de resultados de hardware
// ----------------------------------------------------------------------------

json::object hardware_to_json(const hardware_info& hw) {
  json::object o;
  o["timestamp"] = json::value::from_string(hw.timestamp);
  o["cpu_name"] = json::value::from_string(hw.cpu_name);
  o["physical_cpus"] = json::value::from_int(hw.physical_cpus);
  o["physical_cores"] = json::value::from_int(hw.physical_cores);
  o["logical_processors"] = json::value::from_int(hw.logical_processors);
  o["processor_groups"] = json::value::from_int(hw.processor_groups);
  o["arch_process"] = json::value::from_string(hw.arch_process);
  o["arch_system"] = json::value::from_string(hw.arch_system);
  o["os_name"] = json::value::from_string(hw.os_name);
  o["compiler"] = json::value::from_string(hw.compiler_id);
  o["total_memory_bytes"] = json::value::from_int(hw.total_memory_bytes);
  o["available_memory_bytes"] = json::value::from_int(hw.available_memory_bytes);

  json::array core_arr;
  for (const cpu_core_info& c : hw.core_layout) {
    json::object co;
    co["first_logical_processor"] = json::value::from_int(c.first_logical_processor);
    co["core_index"] = json::value::from_int(c.core_index);
    co["multi_threaded"] = json::value::from_bool(c.multi_threaded);
    core_arr.push_back(json::value::from_object(std::move(co)));
  }
  o["core_layout"] = json::value::from_array(std::move(core_arr));

  json::array thread_arr;
  for (int w : hw.recommended_thread_configs) thread_arr.push_back(json::value::from_int(w));
  o["thread_configs"] = json::value::from_array(std::move(thread_arr));

  json::array process_arr;
  for (int w : hw.recommended_process_configs) process_arr.push_back(json::value::from_int(w));
  o["process_configs"] = json::value::from_array(std::move(process_arr));

  o["configuration_rationale"] = json::value::from_string(hw.configuration_rationale);
  o["limitations"] = json::value::from_string(hw.limitations);
  return o;
}

// CSV vertical (campo,valor,notas): comodo de leer y de mantener.
std::string hardware_to_csv(const hardware_info& hw) {
  auto esc = [](std::string s) -> std::string {
    bool quote = false;
    for (char c : s) if (c == ',' || c == '"' || c == '\n' || c == '\r') { quote = true; break; }
    if (!quote) return s;
    std::string out = "\"";
    for (char c : s) { if (c == '"') out += "\"\""; else out += c; }
    out += "\"";
    return out;
  };
  auto row = [&](const std::string& k, const std::string& v, const std::string& n) {
    return esc(k) + "," + esc(v) + "," + esc(n) + "\n";
  };

  std::ostringstream ss;
  ss << "field,value,notes\n";
  ss << row("timestamp", hw.timestamp, "");
  ss << row("cpu_name", hw.cpu_name, "");
  ss << row("physical_cpus", std::to_string(hw.physical_cpus), "NA si 0");
  ss << row("physical_cores", std::to_string(hw.physical_cores), "NA si 0");
  ss << row("logical_processors", std::to_string(hw.logical_processors), "");
  ss << row("processor_groups", std::to_string(hw.processor_groups), "");
  ss << row("arch_process", hw.arch_process, "");
  ss << row("arch_system", hw.arch_system, "");
  ss << row("os_name", hw.os_name, "");
  ss << row("compiler", hw.compiler_id, "");
  ss << row("total_memory_bytes", std::to_string(hw.total_memory_bytes), "NA si 0");
  ss << row("available_memory_bytes", std::to_string(hw.available_memory_bytes), "NA si 0");
  ss << row("configuration_rationale", hw.configuration_rationale, "razon de cada configuracion");
  ss << row("limitations", hw.limitations, "");
  return ss.str();
}

} // namespace lab
