#ifndef ASDF_STL_HPP
#define ASDF_STL_HPP

#include "asdf_datatype.hpp"

#include <yaml-cpp/yaml.h>

#include <vector>

namespace ASDF {
using namespace std;

// STL

template <typename T>
void yaml_decode(const YAML::Node &node, vector<T> &data) {
  data.reserve(node.size());
  for (YAML::const_iterator ni = node.begin(); ni != node.end(); ++ni) {
    T value;
    yaml_decode(*ni, value);
    data.push_back(std::move(value));
  }
}

template <typename K, typename T>
void yaml_decode(const YAML::Node &node, map<K, T> &data) {
  for (YAML::const_iterator ni = node.begin(); ni != node.end(); ++ni) {
    K key;
    yaml_decode(ni->first, key);
    T value;
    yaml_decode(ni->second, value);
    data[std::move(key)] = std::move(value);
  }
}

template <typename T> YAML::Node yaml_encode(const vector<T> &data) {
  YAML::Node node;
  node.SetStyle(YAML::EmitterStyle::Flow);
  node = data;
  node.SetStyle(YAML::EmitterStyle::Flow);
  return node;
}

template <typename K, typename T>
YAML::Node yaml_encode(const map<K, T> &data) {
  YAML::Node node;
  node = data;
  return node;
}

} // namespace ASDF

#define ASDF_STL_HPP_DONE
#endif // #ifndef ASDF_STL_HPP
#ifndef ASDF_STL_HPP_DONE
#error "Cyclic include depencency"
#endif
