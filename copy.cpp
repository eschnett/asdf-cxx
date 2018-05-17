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
  if (argc != 3) {
    cerr << "Wrong number of arguments\n"
         << "Syntax: " << argv[0] << " <input> <output>\n";
    exit(1);
  }
  string inputfilename = argv[1];
  string outputfilename = argv[2];
  assert(!inputfilename.empty());
  assert(!outputfilename.empty());
  // Read project
  ifstream is(inputfilename, ios::binary | ios::in);
  auto project = ASDF::asdf(is);
  is.close();
  // Copy project
#warning "TODO: Add command line option for this"
  // auto project2 = project;
  // auto project2 = project.copy({});
  auto project2 = project.copy({true, ASDF::block_format_t::inline_array});
  // Write project
  ofstream os(outputfilename, ios::binary | ios::trunc | ios::out);
  project2.write(os);
  os.close();
  //
  cout << "Done.\n";
  return 0;
}
