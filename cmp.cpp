#include "asdf.hpp"

#include <yaml-cpp/yaml.h>

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

int main(int argc, char **argv) {
  cout << "asdf-cmp: Compare the content of two ASDF files\n";
  cout << "Syntax: " << argv[0] << " <file 1> <file 2>\n";
  if (argc != 3) {
    cerr << "Wrong number of arguments\n";
    exit(1);
  }
  string filename1 = argv[1];
  string filename2 = argv[2];
  assert(!filename1.empty());
  assert(!filename2.empty());
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
  auto project = make_shared<ASDF::asdf>(rs, node);
  is.close();
  // Write output
  project->write(outputfilename);
  //
  cout << "Done.\n";
  return 0;
}
