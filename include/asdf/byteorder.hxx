#ifndef ASDF_BYTEORDER_HXX
#define ASDF_BYTEORDER_HXX

#include <asdf/error.hxx>

#include <yaml-cpp/yaml.h>

#include <array>
#include <cassert>
#include <complex>
#include <type_traits>

namespace ASDF {
using namespace std;

// Byte order

enum class byteorder_t { undefined, big, little };

// Is `T` a `std::complex`? Complex numbers are stored as two consecutive
// real components, and each component is byte-swapped on its own; reversing
// all bytes of a complex number would exchange its real and imaginary part.
template <typename T> struct is_complex : false_type {};
template <typename T> struct is_complex<complex<T>> : true_type {};

void yaml_decode(const YAML::Node &node, byteorder_t &byteorder);
YAML::Node yaml_encode(byteorder_t byteorder);
inline ostream &operator<<(ostream &os, byteorder_t byteorder) {
  return os << yaml_encode(byteorder);
}

constexpr uint16_t byteorder_magic = 1;
inline byteorder_t host_byteorder() {
  return reinterpret_cast<const array<unsigned char, 2> &>(
             byteorder_magic)[0] == 1
             ? byteorder_t::little
             : byteorder_t::big;
}

namespace detail {
// Reverse `count` groups of `size` consecutive bytes, each group on its own
inline void reverse_components(unsigned char *data, size_t size, size_t count) {
  for (size_t c = 0; c < count; ++c) {
    unsigned char *const comp = data + c * size;
    for (size_t i = 0; i < size / 2; ++i) {
      const unsigned char tmp = comp[i];
      comp[i] = comp[size - 1 - i];
      comp[size - 1 - i] = tmp;
    }
  }
}
} // namespace detail

// Convert to host byte order
template <typename T>
inline T xtoh(const unsigned char *data, byteorder_t byteorder) {
  if (byteorder == host_byteorder())
    return *reinterpret_cast<const T *>(data);
  array<unsigned char, sizeof(T)> res;
  for (size_t i = 0; i < sizeof(T); ++i)
    res[i] = data[i];
  // A complex number is swapped per component, everything else as a whole
  if constexpr (is_complex<T>::value)
    detail::reverse_components(res.data(), sizeof(T) / 2, 2);
  else
    detail::reverse_components(res.data(), sizeof(T), 1);
  return *reinterpret_cast<const T *>(&res);
}

// Convert from host byte order
template <typename T>
inline array<unsigned char, sizeof(T)> htox(const T &val,
                                            byteorder_t byteorder) {
  array<unsigned char, sizeof(T)> data;
  const unsigned char *ptr = reinterpret_cast<const unsigned char *>(&val);
  for (size_t i = 0; i < sizeof(T); ++i)
    data[i] = ptr[i];
  if (byteorder == host_byteorder())
    return data;
  if constexpr (is_complex<T>::value)
    detail::reverse_components(data.data(), sizeof(T) / 2, 2);
  else
    detail::reverse_components(data.data(), sizeof(T), 1);
  return data;
}

// Swap `N` bytes in place. This is the raw form: it reverses all `N` bytes,
// so a caller converting a complex number must call it once per component.
template <size_t N>
inline void htox(unsigned char *val, byteorder_t byteorder) {
  if (byteorder != host_byteorder()) {
    ASDF_CHECK(byteorder != byteorder_t::undefined, "Byte order is undefined");
    detail::reverse_components(val, N, 1);
  }
}

} // namespace ASDF

#define ASDF_BYTEORDER_HXX_DONE
#endif // #ifndef ASDF_BYTEORDER_HXX
#ifndef ASDF_BYTEORDER_HXX_DONE
#error "Cyclic include depencency"
#endif
