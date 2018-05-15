#include "asdf.hpp"

#include <yaml-cpp/yaml.h>

#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

int main(int argc, char **argv) {
  cout << "asdf-copy: Copy the content of an ASDF file\n";
  cout << "Syntax: " << argv[0] << " <input> <output>\n";
  if (argc != 3) {
    cerr << "Wrong number of arguments\n";
    exit(1);
  }
  string inputfilename = argv[1];
  string outputfilename = argv[2];
  assert(!inputfilename.empty());
  assert(!outputfilename.empty());
  // Read input
  ifstream is(inputfilename, ios::binary | ios::in);
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
  ASDF::reader_state rs(is);
  auto project = ASDF::asdf(rs, node);
  is.close();
  // Write output
  ofstream os(outputfilename, ios::binary | ios::trunc | ios::out);
  project.write(os);
  os.close();
  //
  cout << "Done.\n";
  return 0;
}
