#include <asdf/byteorder.hxx>

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
    ASDF_ERROR("Unknown byte order \"" + str +
               "\"; expected \"big\" or \"little\"");
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
    ASDF_ERROR("Cannot encode an undefined byte order");
  }
  return node;
}

} // namespace ASDF
