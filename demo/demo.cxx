#include <asdf/asdf.hxx>

#include <yaml-cpp/yaml.h>

#include <complex>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using namespace ASDF;

int main(int argc, char **argv) {
  cout << "asdf-demo: Create a simple ASDF file\n";
  ASDF_CHECK_VERSION();

  auto grp = make_shared<group>();

  auto array1d = make_shared<ndarray>(
      vector<int64_t>{1, 2, 3}, block_format_t::block, compression_t::none, 0,
      vector<bool>(), vector<int64_t>{3});
  grp->emplace("beta", array1d);

  auto array2d =
      make_shared<ndarray>(vector<float64_t>{1.0, 2.0, 3.0, 4.0, 5.0, 6.0},
                           block_format_t::inline_array, compression_t::none, 0,
                           vector<bool>(), vector<int64_t>{2, 3});
  grp->emplace("gamma", array2d);

  auto array3d = make_shared<ndarray>(
      vector<complex128_t>{{1, 0}, {-2, 0}, {0, 3}, {-4, 0}, {5, 1}, {6, -1}},
      block_format_t::block, compression_t::bzip2, 9, vector<bool>(),
      vector<int64_t>{1, 2, 3});
  grp->emplace("delta", array3d);

  auto array3db = make_shared<ndarray>(
      vector<complex128_t>{{1, 0}, {-2, 0}, {0, 3}, {-4, 0}, {5, 1}, {6, -1}},
      block_format_t::inline_array, compression_t::bzip2, 9, vector<bool>(),
      vector<int64_t>{1, 2, 3});
  grp->emplace("deltab", array3db);

  auto array8d = make_shared<ndarray>(
      vector<bool8_t>{true}, block_format_t::block, compression_t::zlib, 9,
      vector<bool>(), vector<int64_t>{1, 1, 1, 1, 1, 1, 1, 1});
  grp->emplace("epsilon", array8d);

  auto seq = make_shared<sequence>();
  seq->emplace_back(array1d);
  seq->emplace_back(array2d);
  grp->emplace("zeta", seq);

  auto ref = make_shared<reference>("", vector<string>{"group", "1"});
  grp->emplace("eta", ref);

  vector<uint8_t> data4d;
  data4d.resize(4 * 2 * 16 * 8);
  for (int l = 0; l < 8; ++l)
    for (int k = 0; k < 16; ++k)
      for (int j = 0; j < 2; ++j)
        for (int i = 0; i < 4; ++i)
          data4d.push_back(unsigned(i + 4 * (j + 2 * (k + 16 * l))));
  auto arr4d = make_shared<ndarray>(std::move(data4d), block_format_t::block,
                                    compression_t::zlib, 9, vector<bool>(),
                                    vector<int64_t>{4, 2, 16, 8});
  grp->emplace(
      "attributed",
      make_shared<group>(std::map<string, shared_ptr<entry>>{
          {"null", make_shared<null_entry>()},
          {"fbool", make_shared<bool_entry>(false)},
          {"tbool", make_shared<bool_entry>(true)},
          {"int", make_shared<int_entry>(42)},
          {"float", make_shared<float_entry>(12.3)},
          {"complex", make_shared<complex_entry>(complex64_t(-4.4, -5.5))},
          {"string", make_shared<string_entry>("hello")},
          {"array4d", make_shared<ndarray_entry>(std::move(arr4d))}}));

  auto project = make_shared<asdf>(map<string, string>(), grp);

  project->write("demo.asdf");

  cout << "Done.\n";
  return 0;
}
