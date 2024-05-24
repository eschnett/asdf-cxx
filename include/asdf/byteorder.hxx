#ifndef ASDF_BYTEORDER_HXX
#define ASDF_BYTEORDER_HXX

#include <yaml-cpp/yaml.h>

#include <array>
#include <cassert>

namespace ASDF {
using namespace std;

// Byte order

enum class byteorder_t { undefined, big, little };

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

// Convert to host byte order
template <typename T>
inline T xtoh(const unsigned char *data, byteorder_t byteorder) {
  if (byteorder == host_byteorder())
    return *reinterpret_cast<const T *>(data);
  array<unsigned char, sizeof(T)> res;
  for (size_t i = 0; i < sizeof(T); ++i)
    res[i] = data[sizeof(T) - 1 - i];
  return *reinterpret_cast<const T *>(&res);
}

// Convert from host byte order
template <typename T>
inline array<unsigned char, sizeof(T)> htox(const T &val,
                                            byteorder_t byteorder) {
  auto data = reinterpret_cast<const array<unsigned char, sizeof(T)>>(val);
  if (byteorder == host_byteorder())
    return data;
  array<unsigned char, sizeof(T)> res;
  for (size_t i = 0; i < sizeof(T); ++i)
    res[i] = data[sizeof(T) - 1 - i];
  return res;
}

template <size_t N>
inline void htox(unsigned char *val, byteorder_t byteorder) {
  if (byteorder != host_byteorder()) {
    assert(byteorder != byteorder_t::undefined);
    array<unsigned char, N> tmp;
    for (size_t i = 0; i < N; ++i)
      tmp[i] = val[N - 1 - i];
    *reinterpret_cast<array<unsigned char, N> *>(val) = tmp;
  }
}

} // namespace ASDF

#define ASDF_BYTEORDER_HXX_DONE
#endif // #ifndef ASDF_BYTEORDER_HXX
#ifndef ASDF_BYTEORDER_HXX_DONE
#error "Cyclic include depencency"
#endif
