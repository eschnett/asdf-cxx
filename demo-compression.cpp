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

  auto grp = make_shared<group>();

  auto array3d_none =
      make_shared<ndarray>(data3d, block_format_t::block, compression_t::none,
                           0, std::vector<bool>(), shape);
  grp->emplace("array3d_none", array3d_none);

  if (have_compression_blosc()) {
    auto array3d_blosc = make_shared<ndarray>(data3d, block_format_t::block,
                                              compression_t::blosc, 9,
                                              std::vector<bool>(), shape);
    grp->emplace("array3d_blosc", array3d_blosc);
  }

  if (have_compression_blosc2()) {
    auto array3d_blosc2 = make_shared<ndarray>(data3d, block_format_t::block,
                                               compression_t::blosc2, 9,
                                               std::vector<bool>(), shape);
    grp->emplace("array3d_blosc2", array3d_blosc2);
  }

  if (have_compression_bzip2()) {
    auto array3d_bzip2 = make_shared<ndarray>(data3d, block_format_t::block,
                                              compression_t::bzip2, 9,
                                              std::vector<bool>(), shape);
    grp->emplace("array3d_bzip2", array3d_bzip2);
  }

  if (have_compression_liblz4()) {
    auto array3d_liblz4 = make_shared<ndarray>(data3d, block_format_t::block,
                                               compression_t::liblz4, 9,
                                               std::vector<bool>(), shape);
    grp->emplace("array3d_liblz4", array3d_liblz4);
  }

  if (have_compression_zlib()) {
    auto array3d_zlib =
        make_shared<ndarray>(data3d, block_format_t::block, compression_t::zlib,
                             9, std::vector<bool>(), shape);
    grp->emplace("array3d_zlib", array3d_zlib);
  }

  auto project = make_shared<asdf>(map<string, string>(), grp);

  project->write("compression.asdf");
}

template <typename T>
void read_file(const std::vector<int64_t> &shape,
               const std::vector<T> &data3d) {
  std::cout << "reading file...\n";

  // Read project
  const std::shared_ptr<asdf> project =
      std::make_shared<asdf>("compression.asdf");
  const std::shared_ptr<group> grp = project->get_group();

  for (const auto &[k, v] : *grp->get_group())
    std::cout << "[" << k << "]\n";
  const std::shared_ptr<ndarray> array3d_none =
      grp->at("array3d_none")->get_maybe_ndarray();
  const std::vector<T> data3d_none = array3d_none->get_data_vector<T>();
  if (!data_equal(shape, data3d, data3d_none)) {
    std::cerr << "Dataset \"array3d_none\" is incorrect\n";
    std::exit(1);
  }

  if (have_compression_blosc()) {
    const std::shared_ptr<ndarray> array3d_blosc =
        grp->at("array3d_blosc")->get_maybe_ndarray();
    const std::vector<T> data3d_blosc = array3d_blosc->get_data_vector<T>();
    if (!data_equal(shape, data3d, data3d_blosc)) {
      std::cerr << "Dataset \"array3d_blosc\" is incorrect\n";
      std::exit(1);
    }
  }

  if (have_compression_blosc2()) {
    const std::shared_ptr<ndarray> array3d_blosc2 =
        grp->at("array3d_blosc2")->get_maybe_ndarray();
    const std::vector<T> data3d_blosc2 = array3d_blosc2->get_data_vector<T>();
    if (!data_equal(shape, data3d, data3d_blosc2)) {
      std::cerr << "Dataset \"array3d_blosc2\" is incorrect\n";
      std::exit(1);
    }
  }

  if (have_compression_bzip2()) {
    const std::shared_ptr<ndarray> array3d_bzip2 =
        grp->at("array3d_bzip2")->get_maybe_ndarray();
    const std::vector<T> data3d_bzip2 = array3d_bzip2->get_data_vector<T>();
    if (!data_equal(shape, data3d, data3d_bzip2)) {
      std::cerr << "Dataset \"array3d_bzip2\" is incorrect\n";
      std::exit(1);
    }
  }

  if (have_compression_liblz4()) {
    const std::shared_ptr<ndarray> array3d_liblz4 =
        grp->at("array3d_liblz4")->get_maybe_ndarray();
    const std::vector<T> data3d_liblz4 = array3d_liblz4->get_data_vector<T>();
    if (!data_equal(shape, data3d, data3d_liblz4)) {
      std::cerr << "Dataset \"array3d_liblz4\" is incorrect\n";
      std::exit(1);
    }
  }

  if (have_compression_zlib()) {
    const std::shared_ptr<ndarray> array3d_zlib =
        grp->at("array3d_zlib")->get_maybe_ndarray();
    const std::vector<T> data3d_zlib = array3d_zlib->get_data_vector<T>();
    if (!data_equal(shape, data3d, data3d_zlib)) {
      std::cerr << "Dataset \"array3d_zlib\" is incorrect\n";
      std::exit(1);
    }
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
