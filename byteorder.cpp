#include "asdf_byteorder.hpp"

#include <cassert>

namespace ASDF {

// Byte order

void yaml_decode(const YAML::Node &node, byteorder_t &byteorder) {
  string str = node.Scalar();
  if (str == "big")
    byteorder = byteorder_t::big;
  else if (str == "little")
    byteorder = byteorder_t::little;
  else
    assert(0);
}

YAML::Node yaml_encode(byteorder_t byteorder) {
  YAML::Node node;
  switch (byteorder) {
  case byteorder_t::big:
    node = "big";
    break;
  case byteorder_t::little:
    node = "little";
    break;
  default:
    assert(0);
  }
  return node;
}

} // namespace ASDF
