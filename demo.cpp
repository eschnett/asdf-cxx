#include "asdf.hpp"

#include <complex>
#include <fstream>
#include <iostream>

using namespace std;
using namespace ASDF;

int main(int argc, char **argv) {
  cout << "asdf-demo:\n";
  auto array0d =
      make_shared<ndarray>(vector<int64_t>{42}, block_format_t::block,
                           vector<bool>(), vector<int64_t>{});
  auto col0 = make_shared<column>("alpha", array0d, string());
  auto array1d =
      make_shared<ndarray>(vector<int64_t>{1, 2, 3}, block_format_t::block,
                           vector<bool>(), vector<int64_t>{3});
  auto col1 = make_shared<column>("beta", array1d, string());
  auto array2d = make_shared<ndarray>(vector<float64_t>{1.0, 2.0, 3.0, 4.0},
                                      block_format_t::block, vector<bool>(),
                                      vector<int64_t>{2, 2});
  auto col2 = make_shared<column>("gamma", array2d, string());
  auto array3d = make_shared<ndarray>(
      vector<complex128_t>{1, -2, 3i, -4i, 5 + 1i, 6 - 1i},
      block_format_t::block, vector<bool>(), vector<int64_t>{1, 2, 3});
  auto col3 = make_shared<column>("delta", array3d, string());
  auto array8d = make_shared<ndarray>(vector<bool8_t>{true},
                                      block_format_t::block, vector<bool>(),
                                      vector<int64_t>{1, 1, 1, 1, 1, 1, 1, 1});
  auto col8 = make_shared<column>("epsilon", array8d, string());
  auto tab = make_shared<table>(
      vector<shared_ptr<column>>{col0, col1, col2, col3, col8});
  auto project = asdf(vector<shared_ptr<table>>{tab});
  fstream fout("demo.asdf", ios::binary | ios::trunc | ios::out);
  project.write(fout);
  fout.close();
  cout << "Done.\n";
  return 0;
}
