#include "asdf.hpp"

#include <yaml-cpp/yaml.h>

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

using namespace ASDF;

template <typename T>
std::vector<T> make_data(const std::vector<int64_t> &shape) {
  assert(shape.size() == 3);
  std::vector<T> data3d(shape[0] * shape[1] * shape[2]);
  size_t n = 0;
  for (int k = 0; k < shape[2]; ++k)
    for (int j = 0; j < shape[1]; ++j)
      for (int i = 0; i < shape[0]; ++i)
        data3d[n++] = i + 1000 * j + 1000000 * k;
  assert(n == data3d.size());
  return data3d;
}

template <typename T>
bool data_equal(const std::vector<int64_t> &shape, const std::vector<T> &data1,
                const std::vector<T> &data2) {
  assert(shape.size() == 3);
  const size_t n = shape[0] * shape[1] * shape[2];
  if (data1.size() != n || data2.size() != n)
    return false;
  for (size_t i = 0; i < n; ++i)
    if (data1[i] != data2[i])
      return false;
  return true;
}

template <typename T>
void write_file(const std::vector<int64_t> &shape,
                const std::vector<T> &data3d) {
  std::cout << "writing file...\n";

  auto array3d_none =
      make_shared<ndarray>(data3d, block_format_t::block, compression_t::none,
                           0, std::vector<bool>(), shape);
  auto array3d_blosc =
      make_shared<ndarray>(data3d, block_format_t::block, compression_t::blosc,
                           9, std::vector<bool>(), shape);
  auto array3d_blosc2 =
      make_shared<ndarray>(data3d, block_format_t::block, compression_t::blosc2,
                           9, std::vector<bool>(), shape);
  auto array3d_bzip2 =
      make_shared<ndarray>(data3d, block_format_t::block, compression_t::bzip2,
                           9, std::vector<bool>(), shape);
  auto array3d_zlib =
      make_shared<ndarray>(data3d, block_format_t::block, compression_t::zlib,
                           9, std::vector<bool>(), shape);
  auto grp = make_shared<group>(std::map<std::string, std::shared_ptr<entry>>{
      {"array3d_none",
       std::make_shared<entry>("array3d_none0", array3d_none, string())},
      {"array3d_blosc",
       std::make_shared<entry>("array3d_blosc0", array3d_blosc, string())},
      {"array3d_blosc2",
       std::make_shared<entry>("array3d_blosc20", array3d_blosc2, string())},
      {"array3d_bzip2",
       std::make_shared<entry>("array3d_bzip20", array3d_bzip2, string())},
      {"array3d_zlib",
       std::make_shared<entry>("array3d_zlib0", array3d_zlib, string())},
  });
  auto project = asdf({}, grp);

  project.write("compression.asdf");
}

template <typename T>
void read_file(const std::vector<int64_t> &shape,
               const std::vector<T> &data3d) {
  std::cout << "reading file...\n";

  // Read project
  const std::shared_ptr<asdf> project =
      std::make_shared<asdf>("compression.asdf");
  const std::shared_ptr<group> grp = project->get_group();

  const std::shared_ptr<ndarray> array3d_none =
      grp->get_entries().at("array3d_none")->get_array();
  const std::vector<T> data3d_none = array3d_none->get_data_vector<T>();
  if (!data_equal(shape, data3d, data3d_none)) {
    std::cerr << "Dataset \"array3d_none\" is incorrect\n";
    std::exit(1);
  }

  const std::shared_ptr<ndarray> array3d_blosc =
      grp->get_entries().at("array3d_blosc")->get_array();
  const std::vector<T> data3d_blosc = array3d_blosc->get_data_vector<T>();
  if (!data_equal(shape, data3d, data3d_blosc)) {
    std::cerr << "Dataset \"array3d_blosc\" is incorrect\n";
    std::exit(1);
  }

  const std::shared_ptr<ndarray> array3d_blosc2 =
      grp->get_entries().at("array3d_blosc2")->get_array();
  const std::vector<T> data3d_blosc2 = array3d_blosc2->get_data_vector<T>();
  if (!data_equal(shape, data3d, data3d_blosc2)) {
    std::cerr << "Dataset \"array3d_blosc2\" is incorrect\n";
    std::exit(1);
  }

  const std::shared_ptr<ndarray> array3d_bzip2 =
      grp->get_entries().at("array3d_bzip2")->get_array();
  const std::vector<T> data3d_bzip2 = array3d_bzip2->get_data_vector<T>();
  if (!data_equal(shape, data3d, data3d_bzip2)) {
    std::cerr << "Dataset \"array3d_bzip2\" is incorrect\n";
    std::exit(1);
  }

  const std::shared_ptr<ndarray> array3d_zlib =
      grp->get_entries().at("array3d_zlib")->get_array();
  const std::vector<T> data3d_zlib = array3d_zlib->get_data_vector<T>();
  if (!data_equal(shape, data3d, data3d_zlib)) {
    std::cerr << "Dataset \"array3d_zlib\" is incorrect\n";
    std::exit(1);
  }
}

int main(int argc, char **argv) {
  cout << "asdf-demo: Create a compressed ASDF file\n";

  const std::vector<int64_t> shape{101, 101, 101};
  const auto data = make_data<float64_t>(shape);

  write_file(shape, data);
  read_file(shape, data);

  std::cout << "Done.\n";
  return 0;
}
