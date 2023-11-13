#include <asdf/asdf.hxx>

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

  int64_t ni = 1000, nj = 1000, nk = 250;
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
  const auto
      compression = // have_compression_blosc2()  ? compression_t::blosc2 :
      have_compression_blosc() ? compression_t::blosc : compression_t::zlib;
  const int level = 9;
  auto array3d = make_shared<ndarray>(std::move(rho), block_format_t::block,
                                      compression, level, std::vector<bool>(),
                                      std::vector<int64_t>{ni, nj, nk});
  assert(rho.empty());
  auto grp = make_shared<group>();
  grp->emplace("rho", array3d);
  auto project = make_shared<asdf>(map<string, string>(), grp);
  cout << "\n";

  cout << "  writing project..." << flush;
  project->write("large.asdf");
  cout << "\n";

  cout << "Done.\n";

  return 0;
}
