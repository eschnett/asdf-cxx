#include "asdf.hpp"

#include <complex>
#include <fstream>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

using namespace std;
using namespace ASDF;

int main(int argc, char **argv) {
  cout << "asdf-demo-large: Create a large ASDF file\n";

  int64_t ni = 1000, nj = 1000, nk = 500;
  auto getidx = [&](int64_t i, int64_t j, int64_t k) {
    return (i * nj + j) * nk + k;
  };
  int64_t npoints = ni * nj * nk;

  cout << "  allocating..." << flush;
  vector<float64_t> rho(npoints);
  cout << "\n";

  cout << "  initializing..." << flush;
  for (int64_t i = 0; i < ni; ++i)
    for (int64_t j = 0; j < nj; ++j)
      for (int64_t k = 0; k < nk; ++k) {
        int64_t idx = getidx(i, j, k);
        rho.at(idx) = 1.0 / (i + j + k + 1);
      }
  cout << "\n";

  cout << "  creating project..." << flush;
  auto array3d = make_shared<ndarray>(std::move(rho), block_format_t::block,
                                      compression_t::zlib, 9, vector<bool>(),
                                      vector<int64_t>{ni, nj, nk});
  assert(rho.empty());
  auto ent = make_shared<entry>("rho", array3d, string());
  auto grp = make_shared<group>(map<string, shared_ptr<entry>>{{"rho", ent}});
  auto project = asdf({}, grp);
  cout << "\n";

  cout << "  writing project..." << flush;
  fstream os("large.asdf", ios::binary | ios::trunc | ios::out);
  project.write(os);
  os.close();
  cout << "\n";

  cout << "Done.\n";

  return 0;
}
