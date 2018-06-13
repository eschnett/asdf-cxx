#include "asdf.hpp"

#include <yaml-cpp/yaml.h>

#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

using namespace ASDF;
using namespace std;

int main(int argc, char **argv) {
  cout << "asdf-copy: Copy the content of an ASDF file\n";

  // Parse command line arguments
  auto check = [=](bool cond, const string &msg) {
    if (cond)
      return;
    cerr << msg << "Syntax: " << argv[0]
         << " [--array=(blockinline)] [--compression=(none|bzip2|zlib)] <input "
            "file> <output file>\n"
         << "Aborting.\n";
    exit(1);
  };
  block_format_t block_format = block_format_t::undefined;
  compression_t compression = compression_t::undefined;
  vector<string> args;
  for (int argi = 1; argi < argc; ++argi)
    args.push_back(argv[argi]);
  while (!args.empty() && !args.at(0).empty() && args.at(0)[0] == '-') {
    const auto &opt = args.at(0);
    if (opt == "--array=block") {
      check(block_format == block_format_t::undefined,
            "Array format already set\n");
      block_format = block_format_t::block;
    } else if (opt == "--array=inline") {
      check(block_format == block_format_t::undefined,
            "Array format already set\n");
      block_format = block_format_t::inline_array;
    } else if (opt == "--compression=none") {
      check(compression == compression_t::undefined,
            "Compression type already set\n");
      compression = compression_t::none;
    } else if (opt == "--compression=bzip2") {
      check(compression == compression_t::undefined,
            "Compression type already set\n");
      compression = compression_t::bzip2;
    } else if (opt == "--compression=zlib") {
      check(compression == compression_t::undefined,
            "Compression type already set\n");
      compression = compression_t::zlib;
    } else {
      assert(0);
    }
    args.erase(args.begin());
  }
  check(args.size() == 2, "Wrong number of arguments\n");
  const string &inputfilename = args.at(0);
  const string &outputfilename = args.at(1);
  check(!inputfilename.empty(), "Input file name is empty\n");
  check(!outputfilename.empty(), "Output file name is empty\n");

  // Read project
  auto pis = make_shared<ifstream>(inputfilename, ios::binary | ios::in);
  auto project = asdf(pis);
  pis.reset();

  // Copy project
  const copy_state cs{block_format != block_format_t::undefined, block_format,
                      compression != compression_t::undefined, compression};
  auto project2 = project.copy(cs);

  // Write project
  ofstream os(outputfilename, ios::binary | ios::trunc | ios::out);
  project2.write(os);
  os.close();

  cout << "Done.\n";
  return 0;
}
