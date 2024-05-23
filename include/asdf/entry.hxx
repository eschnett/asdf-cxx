#ifndef ASDF_ENTRY_HXX
#define ASDF_ENTRY_HXX

#include <asdf/datatype.hxx>
#include <asdf/ndarray.hxx>
#include <asdf/reference.hxx>

#include <yaml-cpp/yaml.h>

#include <array>
#include <complex>
#include <exception>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <cstdint>

namespace ASDF {
using namespace std;

// Entry

enum class entry_type_t {
  unknown,
  null,
  bool8, // TrueFalseBool
  int64,
  float64,
  complex128,
  string,
  software,
  history_entry,
  ndarray,
  reference,
  sequence,
  group,
};

std::ostream &operator<<(std::ostream &os, entry_type_t entry_type);

class null_entry;
class bool_entry;
class int_entry;
class float_entry;
class complex128_entry;
class string_entry;
class software;
// class history_entry;
class ndarray_entry;
class reference_entry;
class sequence;
class group;

class entry {
public:
  virtual ~entry() {}

  virtual entry_type_t get_entry_type() const = 0;

  virtual std::shared_ptr<entry> copy(const copy_state &cs) const = 0;

  virtual writer &to_yaml(writer &w) const = 0;
  friend writer &operator<<(writer &w, const entry &ent) {
    return ent.to_yaml(w);
  }
  friend writer &operator<<(writer &w, const std::shared_ptr<entry> &ent) {
    return ent->to_yaml(w);
  }

  virtual std::optional<std::tuple<>> get_maybe_null() const { return {}; }
  virtual std::optional<bool> get_maybe_bool() const { return {}; }
  virtual std::optional<std::int64_t> get_maybe_int() const { return {}; }
  virtual std::optional<float64_t> get_maybe_float() const { return {}; }
  virtual std::optional<std::complex<float64_t>> get_maybe_complex() const {
    return {};
  }
  virtual std::optional<std::string> get_maybe_string() const { return {}; }
  virtual std::optional<std::array<std::string, 4>> get_maybe_software() const {
    return {};
  }
  virtual std::shared_ptr<ndarray> get_maybe_ndarray() const { return {}; }
  virtual std::shared_ptr<reference> get_maybe_reference() const { return {}; }
  virtual std::shared_ptr<std::vector<std::shared_ptr<entry>>>
  get_maybe_sequence() const {
    return {};
  }
  virtual std::shared_ptr<std::map<std::string, std::shared_ptr<entry>>>
  get_maybe_group() const {
    return {};
  }
};

class null_entry : public entry {
  std::tuple<> value;

public:
  using value_type = std::tuple<>;

  // null_entry() = delete;
  null_entry(const null_entry &) = default;
  null_entry(null_entry &&) = default;
  null_entry &operator=(const null_entry &) = default;
  null_entry &operator=(null_entry &&) = default;

  virtual ~null_entry() {}

  null_entry(std::tuple<> value) : value(std::move(value)) {}
  null_entry() : null_entry(std::tuple<>()) {}

  virtual entry_type_t get_entry_type() const override {
    return entry_type_t::null;
  }

  virtual std::shared_ptr<entry> copy(const copy_state &cs) const override {
    return std::make_shared<null_entry>();
  }

  virtual writer &to_yaml(writer &w) const override;
  friend writer &operator<<(writer &w, const null_entry &ent) {
    return ent.to_yaml(w);
  }

  virtual std::optional<std::tuple<>> get_maybe_null() const override {
    return value;
  }

  std::tuple<> get_null() const { return value; }
};

class bool_entry : public entry {
  bool value;

public:
  using value_type = bool;

  bool_entry() = delete;
  bool_entry(const bool_entry &) = default;
  bool_entry(bool_entry &&) = default;
  bool_entry &operator=(const bool_entry &) = default;
  bool_entry &operator=(bool_entry &&) = default;

  virtual ~bool_entry() {}

  bool_entry(bool value) : value(std::move(value)) {}

  virtual entry_type_t get_entry_type() const override {
    return entry_type_t::bool8;
  }

  virtual std::shared_ptr<entry> copy(const copy_state &cs) const override {
    return std::make_shared<bool_entry>(value);
  }

  virtual writer &to_yaml(writer &w) const override;
  friend writer &operator<<(writer &w, const bool_entry &ent) {
    return ent.to_yaml(w);
  }

  virtual std::optional<bool> get_maybe_bool() const override { return value; }

  bool get_bool() const { return value; }
};

class int_entry : public entry {
  std::int64_t value;

public:
  using value_type = std::int64_t;

  int_entry() = delete;
  int_entry(const int_entry &) = default;
  int_entry(int_entry &&) = default;
  int_entry &operator=(const int_entry &) = default;
  int_entry &operator=(int_entry &&) = default;

  virtual ~int_entry() {}

  int_entry(std::int64_t value) : value(std::move(value)) {}

  virtual entry_type_t get_entry_type() const override {
    return entry_type_t::int64;
  }

  virtual std::shared_ptr<entry> copy(const copy_state &cs) const override {
    return std::make_shared<int_entry>(value);
  }

  virtual writer &to_yaml(writer &w) const override;
  friend writer &operator<<(writer &w, const int_entry &ent) {
    return ent.to_yaml(w);
  }

  virtual std::optional<std::int64_t> get_maybe_int() const override {
    return value;
  }

  std::int64_t get_int() const { return value; }
};

class float_entry : public entry {
  float64_t value;

public:
  using value_type = float64_t;

  float_entry() = delete;
  float_entry(const float_entry &) = default;
  float_entry(float_entry &&) = default;
  float_entry &operator=(const float_entry &) = default;
  float_entry &operator=(float_entry &&) = default;

  virtual ~float_entry() {}

  float_entry(float64_t value) : value(std::move(value)) {}

  virtual entry_type_t get_entry_type() const override {
    return entry_type_t::float64;
  }

  virtual std::shared_ptr<entry> copy(const copy_state &cs) const override {
    return std::make_shared<float_entry>(value);
  }

  virtual writer &to_yaml(writer &w) const override;
  friend writer &operator<<(writer &w, const float_entry &ent) {
    return ent.to_yaml(w);
  }

  virtual std::optional<float64_t> get_maybe_float() const override {
    return value;
  }

  float64_t get_float() const { return value; }
};

class complex_entry : public entry {
  std::complex<float64_t> value;

public:
  using value_type = std::complex<float64_t>;

  complex_entry() = delete;
  complex_entry(const complex_entry &) = default;
  complex_entry(complex_entry &&) = default;
  complex_entry &operator=(const complex_entry &) = default;
  complex_entry &operator=(complex_entry &&) = default;

  virtual ~complex_entry() {}

  complex_entry(std::complex<float64_t> value) : value(std::move(value)) {}

  virtual entry_type_t get_entry_type() const override {
    return entry_type_t::complex128;
  }

  virtual std::shared_ptr<entry> copy(const copy_state &cs) const override {
    return std::make_shared<complex_entry>(value);
  }

  virtual writer &to_yaml(writer &w) const override;
  friend writer &operator<<(writer &w, const complex_entry &ent) {
    return ent.to_yaml(w);
  }

  virtual std::optional<std::complex<float64_t>>
  get_maybe_complex() const override {
    return value;
  }

  std::complex<float64_t> get_complex() const { return value; }
};

class string_entry : public entry {
  std::string value;

public:
  using value_type = std::string;

  string_entry() = delete;
  string_entry(const string_entry &) = default;
  string_entry(string_entry &&) = default;
  string_entry &operator=(const string_entry &) = default;
  string_entry &operator=(string_entry &&) = default;

  virtual ~string_entry() {}

  string_entry(std::string value) : value(std::move(value)) {}

  virtual entry_type_t get_entry_type() const override {
    return entry_type_t::string;
  }

  virtual std::shared_ptr<entry> copy(const copy_state &cs) const override {
    return std::make_shared<string_entry>(value);
  }

  virtual writer &to_yaml(writer &w) const override;
  friend writer &operator<<(writer &w, const string_entry &ent) {
    return ent.to_yaml(w);
  }

  virtual std::optional<std::string> get_maybe_string() const override {
    return value;
  }

  std::string get_string() const { return value; }
};

class software : public entry {
  std::string name;
  std::string author;
  std::string homepage;
  std::string version;

public:
  using value_type = std::array<std::string, 4>;

  software() = delete;
  software(const software &) = default;
  software(software &&) = default;
  software &operator=(const software &) = default;
  software &operator=(software &&) = default;

  virtual ~software() {}

  software(std::string name, std::string author, std::string homepage,
           std::string version)
      : name(std::move(name)), author(std::move(author)),
        homepage(std::move(homepage)), version(std::move(version)) {}

  software(const std::shared_ptr<reader_state> &rs, const YAML::Node node);
  software(const copy_state &cs, const software &soft);

  virtual entry_type_t get_entry_type() const override {
    return entry_type_t::software;
  }

  virtual std::shared_ptr<entry> copy(const copy_state &cs) const override {
    return std::make_shared<software>(cs, *this);
  }

  virtual writer &to_yaml(writer &w) const override;
  friend writer &operator<<(writer &w, const software &ent) {
    return ent.to_yaml(w);
  }

  virtual std::optional<std::array<std::string, 4>>
  get_maybe_software() const override {
    return {{name, author, homepage, version}};
  }

  std::string get_name() const { return name; }
  std::string get_author() const { return author; }
  std::string get_homepage() const { return homepage; }
  std::string get_version() const { return version; }
  std::array<std::string, 4> get_software() const {
    return {name, author, homepage, version};
  }
};

// class history_entry : public entry {};

class ndarray_entry : public entry {
  std::shared_ptr<ndarray> value;

public:
  using value_type = std::shared_ptr<ndarray>;

  ndarray_entry() = delete;
  ndarray_entry(const ndarray_entry &) = default;
  ndarray_entry(ndarray_entry &&) = default;
  ndarray_entry &operator=(const ndarray_entry &) = default;
  ndarray_entry &operator=(ndarray_entry &&) = default;

  virtual ~ndarray_entry() {}

  ndarray_entry(std::shared_ptr<ndarray> value) : value(std::move(value)) {}
  ndarray_entry(ndarray value)
      : ndarray_entry(std::make_shared<ndarray>(std::move(value))) {}

  ndarray_entry(const std::shared_ptr<reader_state> &rs, const YAML::Node node);
  ndarray_entry(const copy_state &cs, const ndarray_entry &arr);

  virtual entry_type_t get_entry_type() const override {
    return entry_type_t::ndarray;
  }

  virtual std::shared_ptr<entry> copy(const copy_state &cs) const override {
    return std::make_shared<ndarray_entry>(cs, *this);
  }

  virtual writer &to_yaml(writer &w) const override;
  friend writer &operator<<(writer &w, const ndarray_entry &ent) {
    return ent.to_yaml(w);
  }

  virtual std::shared_ptr<ndarray> get_maybe_ndarray() const override {
    return value;
  }

  std::shared_ptr<ndarray> get_ndarray() const { return value; }
};

class reference_entry : public entry {
  std::shared_ptr<reference> value;

public:
  using value_type = std::shared_ptr<reference>;

  reference_entry() = delete;
  reference_entry(const reference_entry &) = default;
  reference_entry(reference_entry &&) = default;
  reference_entry &operator=(const reference_entry &) = default;
  reference_entry &operator=(reference_entry &&) = default;

  virtual ~reference_entry() {}

  reference_entry(std::shared_ptr<reference> value) : value(std::move(value)) {}
  reference_entry(reference value)
      : reference_entry(std::make_shared<reference>(std::move(value))) {}

  reference_entry(const std::shared_ptr<reader_state> &rs,
                  const YAML::Node node);
  reference_entry(const copy_state &cs, const reference_entry &arr);

  virtual entry_type_t get_entry_type() const override {
    return entry_type_t::reference;
  }

  virtual std::shared_ptr<entry> copy(const copy_state &cs) const override {
    return std::make_shared<reference_entry>(cs, *this);
  }

  virtual writer &to_yaml(writer &w) const override;
  friend writer &operator<<(writer &w, const reference_entry &ent) {
    return ent.to_yaml(w);
  }

  virtual std::shared_ptr<reference> get_maybe_reference() const override {
    return value;
  }

  std::shared_ptr<reference> get_reference() const { return value; }
};

class sequence : public entry {
  std::shared_ptr<std::vector<std::shared_ptr<entry>>> entries;

public:
  using value_type = std::shared_ptr<std::vector<std::shared_ptr<entry>>>;

  // sequence() = delete;
  sequence(const sequence &) = default;
  sequence(sequence &&) = default;
  sequence &operator=(const sequence &) = default;
  sequence &operator=(sequence &&) = default;

  virtual ~sequence() {}

  sequence(std::shared_ptr<std::vector<std::shared_ptr<entry>>> entries)
      : entries(std::move(entries)) {}
  sequence(std::vector<std::shared_ptr<entry>> entries)
      : sequence(std::make_shared<std::vector<std::shared_ptr<entry>>>(
            std::move(entries))) {}
  sequence() : sequence(std::vector<std::shared_ptr<entry>>()) {}

  sequence(const shared_ptr<reader_state> &rs, const YAML::Node &node);
  sequence(const copy_state &cs, const sequence &from);

  // template <typename T>
  // sequence(const std::vector<std::shared_ptr<T>> &data,
  //          const std::function<entry(const T &)> &f) {
  //   for (const auto &v : data)
  //     entries->push_back(f(*v));
  // }

  virtual entry_type_t get_entry_type() const override {
    return entry_type_t::sequence;
  }

  virtual std::shared_ptr<entry> copy(const copy_state &cs) const override {
    return std::make_shared<sequence>(cs, *this);
  }

  virtual writer &to_yaml(writer &w) const override;
  friend writer &operator<<(writer &w, const sequence &ent) {
    return ent.to_yaml(w);
  }

  virtual std::shared_ptr<std::vector<std::shared_ptr<entry>>>
  get_maybe_sequence() const override {
    return entries;
  }

  void push_back(std::shared_ptr<entry> value) {
    entries->push_back(std::move(value));
  }
  template <typename T> void emplace_back(T &&value) {
    entries->push_back(make_entry(std::forward<T>(value)));
  }

  std::size_t size() const { return entries->size(); }
  std::shared_ptr<entry> at(const std::size_t n) const {
    return entries->at(n);
  }
  std::shared_ptr<std::vector<std::shared_ptr<entry>>> get_sequence() const {
    return entries;
  }
};

class group : public entry, public std::enable_shared_from_this<group> {
  std::shared_ptr<std::map<std::string, std::shared_ptr<entry>>> entries;

public:
  using value_type =
      std::shared_ptr<std::map<std::string, std::shared_ptr<entry>>>;

  // group() = delete;
  group(const group &) = default;
  group(group &&) = default;
  group &operator=(const group &) = default;
  group &operator=(group &&) = default;

  virtual ~group() {}

  group(std::shared_ptr<std::map<std::string, std::shared_ptr<entry>>> entries)
      : entries(std::move(entries)) {}
  group(std::map<std::string, std::shared_ptr<entry>> entries)
      : group(std::make_shared<std::map<std::string, std::shared_ptr<entry>>>(
            std::move(entries))) {}
  group() : group(std::map<std::string, std::shared_ptr<entry>>()) {}

  template <typename T>
  group(const map<string, shared_ptr<T>> &data,
        const function<entry(const T &)> &f) {
    for (const auto &kv : data)
      entries[kv.first] = f(*kv.second);
  }

  group(const shared_ptr<reader_state> &rs, const YAML::Node &node);
  group(const copy_state &cs, const group &grp);

  virtual entry_type_t get_entry_type() const override {
    return entry_type_t::group;
  }

  virtual std::shared_ptr<entry> copy(const copy_state &cs) const override {
    return std::make_shared<group>(cs, *this);
  }

  virtual writer &to_yaml(writer &w) const override;
  friend writer &operator<<(writer &w, const group &ent) {
    return ent.to_yaml(w);
  }

  virtual std::shared_ptr<std::map<std::string, std::shared_ptr<entry>>>
  get_maybe_group() const override {
    return entries;
  }

  void insert(std::pair<const std::string, std::shared_ptr<entry>> key_value) {
    entries->insert(std::move(key_value));
  }
  void insert(const std::string &key, std::shared_ptr<entry> value) {
    entries->emplace(key, std::move(value));
  }
  template <typename T> void emplace(const std::string &key, T &&value) {
    entries->emplace(key, make_entry(std::forward<T>(value)));
  }
  std::size_t count(const std::string &key) const {
    return entries->count(key);
  }
  std::shared_ptr<entry> at(const std::string &key) const {
    return entries->at(key);
  }
  std::shared_ptr<std::map<std::string, std::shared_ptr<entry>>>
  get_group() const {
    return entries;
  }
};

inline std::shared_ptr<null_entry> make_entry(std::tuple<> value) {
  return std::make_shared<null_entry>(std::move(value));
}
inline std::shared_ptr<bool_entry> make_entry(bool value) {
  return std::make_shared<bool_entry>(value);
}
template <typename T>
std::enable_if_t<std::is_integral_v<T>, std::shared_ptr<int_entry>>
make_entry(T value) {
  return std::make_shared<int_entry>(std::int64_t(std::move(value)));
}
template <typename T>
std::enable_if_t<std::is_floating_point_v<T>, std::shared_ptr<float_entry>>
make_entry(T value) {
  return std::make_shared<float_entry>(float64_t(std::move(value)));
}
template <typename T>
std::enable_if_t<std::is_floating_point_v<T>, std::shared_ptr<complex_entry>>
make_entry(std::complex<T> value) {
  return std::make_shared<complex_entry>(
      std::complex<float64_t>(std::move(value)));
}
inline std::shared_ptr<string_entry> make_entry(std::string value) {
  return std::make_shared<string_entry>(std::move(value));
}
inline std::shared_ptr<ndarray_entry> make_entry(ndarray value) {
  return std::make_shared<ndarray_entry>(std::move(value));
}
inline std::shared_ptr<reference_entry> make_entry(reference value) {
  return std::make_shared<reference_entry>(std::move(value));
}
inline std::shared_ptr<sequence>
make_entry(std::vector<std::shared_ptr<entry>> entries) {
  return std::make_shared<sequence>(std::move(entries));
}
inline std::shared_ptr<group>
make_entry(std::map<std::string, std::shared_ptr<entry>> entries) {
  return std::make_shared<group>(std::move(entries));
}

inline std::shared_ptr<null_entry>
make_entry(const std::shared_ptr<std::tuple<>> &value) {
  return std::make_shared<null_entry>(*value);
}
inline std::shared_ptr<bool_entry>
make_entry(const std::shared_ptr<bool> &value) {
  return std::make_shared<bool_entry>(*value);
}
template <typename T>
std::enable_if_t<std::is_integral_v<T>, std::shared_ptr<int_entry>>
make_entry(const std::shared_ptr<T> &value) {
  return std::make_shared<int_entry>(std::int64_t(*value));
}
template <typename T>
std::enable_if_t<std::is_floating_point_v<T>, std::shared_ptr<float_entry>>
make_entry(const std::shared_ptr<T> &value) {
  return std::make_shared<float_entry>(float64_t(*value));
}
template <typename T>
std::enable_if_t<std::is_floating_point_v<T>, std::shared_ptr<complex_entry>>
make_entry(const std::shared_ptr<std::complex<T>> &value) {
  return std::make_shared<complex_entry>(std::complex<float64_t>(*value));
}
inline std::shared_ptr<string_entry>
make_entry(const std::shared_ptr<std::string> &value) {
  return std::make_shared<string_entry>(*value);
}
inline std::shared_ptr<ndarray_entry>
make_entry(std::shared_ptr<ndarray> value) {
  return std::make_shared<ndarray_entry>(std::move(value));
}
inline std::shared_ptr<reference_entry>
make_entry(std::shared_ptr<reference> value) {
  return std::make_shared<reference_entry>(std::move(value));
}
inline std::shared_ptr<sequence>
make_entry(std::shared_ptr<std::vector<std::shared_ptr<entry>>> entries) {
  return std::make_shared<sequence>(std::move(entries));
}
inline std::shared_ptr<group> make_entry(
    std::shared_ptr<std::map<std::string, std::shared_ptr<entry>>> entries) {
  return std::make_shared<group>(std::move(entries));
}
inline std::shared_ptr<entry> make_entry(std::shared_ptr<entry> value) {
  return value;
}

std::shared_ptr<entry> make_entry(const std::shared_ptr<reader_state> &rs,
                                  const YAML::Node &node);

} // namespace ASDF

#define ASDF_ENTRY_HXX_DONE
#endif // #ifndef ASDF_ENTRY_HXX
#ifndef ASDF_ENTRY_HXX_DONE
#error "Cyclic include depencency"
#endif
