#include "asdf.hpp"

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

using namespace std;
using namespace ASDF;

void write_external() {
  cout << "Writing external file...\n";

  // The actual dataset
  auto alpha = make_shared<ndarray>(
      vector<int64_t>{1, 2, 3}, block_format_t::inline_array,
      compression_t::none, 0, vector<bool>(), vector<int64_t>{3});
  // A local reference
  auto beta = make_shared<reference>("", vector<string>{"alpha"});

  auto grp = make_shared<group>();
  grp->emplace("alpha", alpha);
  grp->emplace("beta", beta);
  auto project = make_shared<asdf>(map<string, string>(), grp);

  project->write("external.asdf");
}

void write_metadata() {
  cout << "Writing metadata file...\n";

  // A remote reference
  auto gamma = make_shared<reference>("external.asdf", vector<string>{"alpha"});
  // A local reference
  auto delta = make_shared<reference>("", vector<string>{"gamma"});
  // A remote reference to a local reference
  auto epsilon =
      make_shared<reference>("external.asdf", vector<string>{"beta"});

  auto grp = make_shared<group>();
  grp->emplace("gamma", gamma);
  grp->emplace("delta", delta);
  grp->emplace("epsilon", epsilon);
  auto project = make_shared<asdf>(map<string, string>(), grp);

  project->write("metadata.asdf");
}

template <typename T> vector<T> read_array(const shared_ptr<ndarray> &arr) {
  auto datatype = arr->get_datatype();
  assert(datatype->is_scalar);
  assert(datatype->scalar_type_id == get_scalar_type_id<T>());
  auto shape = arr->get_shape();
  assert(shape.size() == 1);
  auto npoints = shape.at(0);
  const memoized<block_t> blk = arr->get_data();
  const void *ptr = blk->ptr();
  size_t nbytes = blk->nbytes();
  assert(nbytes == npoints * sizeof(T));
  vector<T> data(npoints);
  for (size_t i = 0; i < npoints; ++i)
    data.at(i) = static_cast<const int64_t *>(ptr)[i];
  return data;
}

void read_metadata() {
  cout << "Reading metadata file...\n";

  auto project = make_shared<ASDF::asdf>("metadata.asdf");

  auto grp = project->get_group();

  {
    auto gamma = grp->at("gamma")->get_maybe_reference();
    cout << "gamma: <" << gamma->get_target() << ">\n";
    auto rs_node = gamma->resolve();
    auto arr = make_shared<ndarray>(rs_node.first, rs_node.second);
    cout << "gamma': [ndarray] " << yaml_encode(read_array<int64_t>(arr))
         << "\n";
  }

  {
    auto delta = grp->at("delta")->get_maybe_reference();
    cout << "delta: <" << delta->get_target() << ">\n";
    auto rs_node = delta->resolve();
    auto ref = make_shared<reference>(rs_node.first, rs_node.second);
    cout << "delta': [reference] " << ref->get_target() << "\n";
    auto rs_node1 = ref->resolve();
    auto arr = make_shared<ndarray>(rs_node1.first, rs_node1.second);
    cout << "delta'': [ndarray] " << yaml_encode(read_array<int64_t>(arr))
         << "\n";
  }

  {
    auto epsilon = grp->at("epsilon")->get_maybe_reference();
    cout << "epsilon: <" << epsilon->get_target() << ">\n";
    auto rs_node = epsilon->resolve();
    auto ref = make_shared<reference>(rs_node.first, rs_node.second);
    cout << "epsilon': [reference] " << ref->get_target() << "\n";
    auto rs_node1 = ref->resolve();
    auto arr = make_shared<ndarray>(rs_node1.first, rs_node1.second);
    cout << "epsilon'': [ndarray] " << yaml_encode(read_array<int64_t>(arr))
         << "\n";
  }
}

int main(int argc, char **argv) {
  cout << "asdf-demo-external: Create an ASDF file with external references\n";

  write_external();
  write_metadata();
  read_metadata();

  cout << "Done.\n";
  return 0;
}
