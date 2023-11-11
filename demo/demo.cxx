#include <asdf/asdf.hxx>

#include <yaml-cpp/yaml.h>

#include <complex>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <vector>

using namespace std;
using namespace ASDF;

int main(int argc, char **argv) {
  cout << "asdf-demo: Create a simple ASDF file\n";

  auto grp = make_shared<group>();

  auto array0d = make_shared<ndarray>(
      vector<int64_t>{42}, block_format_t::inline_array, compression_t::none, 0,
      vector<bool>(), vector<int64_t>{});
  grp->emplace("alpha", array0d);

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
  seq->emplace_back(array0d);
  seq->emplace_back(array1d);
  seq->emplace_back(array2d);
  grp->emplace("zeta", seq);

  auto ref = make_shared<reference>("", vector<string>{"group", "1"});
  grp->emplace("eta", ref);

  auto project = make_shared<asdf>(map<string, string>(), grp);

  project->write("demo.asdf");

  cout << "Done.\n";
  return 0;
}
