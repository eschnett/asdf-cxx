#include "asdf.hpp"

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

  auto array0d = make_shared<ndarray>(
      vector<int64_t>{42}, block_format_t::inline_array, compression_t::none, 0,
      vector<bool>(), vector<int64_t>{});
  auto ent0 = make_shared<entry>("alpha", array0d, string());
  auto array1d = make_shared<ndarray>(
      vector<int64_t>{1, 2, 3}, block_format_t::block, compression_t::none, 0,
      vector<bool>(), vector<int64_t>{3});
  auto ent1 = make_shared<entry>("beta", array1d, string());
  auto array2d =
      make_shared<ndarray>(vector<float64_t>{1.0, 2.0, 3.0, 4.0, 5.0, 6.0},
                           block_format_t::inline_array, compression_t::none, 0,
                           vector<bool>(), vector<int64_t>{2, 3});
  auto ent2 = make_shared<entry>("gamma", array2d, string());
  auto array3d = make_shared<ndarray>(
      vector<complex128_t>{{1, 0}, {-2, 0}, {0, 3}, {-4, 0}, {5, 1}, {6, -1}},
      block_format_t::block, compression_t::bzip2, 9, vector<bool>(),
      vector<int64_t>{1, 2, 3});
  auto ent3 = make_shared<entry>("delta", array3d, string());
  auto array8d = make_shared<ndarray>(
      vector<bool8_t>{true}, block_format_t::block, compression_t::zlib, 9,
      vector<bool>(), vector<int64_t>{1, 1, 1, 1, 1, 1, 1, 1});
  auto ent8 = make_shared<entry>("epsilon", array8d, string());
  auto seq = make_shared<sequence>(vector<shared_ptr<entry>>{ent0, ent1, ent2});
  auto ents = make_shared<entry>("zeta", seq, string());
  auto ref = make_shared<reference>("", vector<string>{"group", "1"});
  auto entr = make_shared<entry>("eta", ref, string());
  auto grp =
      make_shared<group>(map<string, shared_ptr<entry>>{{"alpha", ent0},
                                                        {"beta", ent1},
                                                        {"gamma", ent2},
                                                        {"delta", ent3},
                                                        {"epsilon", ent8},
                                                        {"zeta", ents},
                                                        {"eta", entr}});
  auto project = asdf({}, grp);

  project.write("demo.asdf");

  cout << "Done.\n";
  return 0;
}
