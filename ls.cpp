#include "asdf.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace ASDF;
using namespace std;

int main(int argc, char **argv) {
  cout << "asdf-ls: List content of ASDF files\n";
  // cout << "Syntax: " << argv[0] << " <filename>+\n";

#if 0
  for (int arg = 1; arg < argc; ++arg) {
    string filename = argv[arg];
    assert(!filename.empty());
    fstream is(filename, ios::binary | ios::in);
    // TODO: stream the file instead
    ostringstream doc;
    for (;;) {
      string line;
      getline(is, line);
      doc << line << "\n";
      if (line == "...")
        break;
    }
    YAML::Node node = YAML::Load(doc.str());
    is.close();
    cout << node << "\n";
  }
#endif

  for (int arg = 1; arg < argc; ++arg) {
    string filename = argv[arg];
    assert(!filename.empty());

    // Read project
    ifstream is(filename, ios::binary | ios::in);
    auto node = asdf::from_yaml(is);

    // Output project
    cout << node << "\n";

    // TODO: Output block info as well
  }

  cout << "Done.\n";
  return 0;
}
