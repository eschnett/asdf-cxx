#include "asdf.hpp"

#include <yaml-cpp/yaml.h>

#include <cassert>
#include <cstdlib>
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
         << " [--array=(blockinline)] [--compression=(none|bzip2|zlib)] "
            "[--compression-level=[0-9]] <input file> <output file>\n"
         << "Aborting.\n";
    exit(1);
  };
  block_format_t block_format = block_format_t::undefined;
  compression_t compression = compression_t::undefined;
  int compression_level = -1;
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
    } else if (opt == "--compression-level=0") { // Dont' judge me for this
      compression_level = 0;
    } else if (opt == "--compression-level=1") {
      compression_level = 1;
    } else if (opt == "--compression-level=2") {
      compression_level = 2;
    } else if (opt == "--compression-level=3") {
      compression_level = 3;
    } else if (opt == "--compression-level=4") {
      compression_level = 4;
    } else if (opt == "--compression-level=5") {
      compression_level = 5;
    } else if (opt == "--compression-level=6") {
      compression_level = 6;
    } else if (opt == "--compression-level=7") {
      compression_level = 7;
    } else if (opt == "--compression-level=8") {
      compression_level = 8;
    } else if (opt == "--compression-level=9") {
      compression_level = 9;
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
  auto project = asdf(inputfilename);

  // Copy project
  const copy_state cs{block_format != block_format_t::undefined,
                      block_format,
                      compression != compression_t::undefined,
                      compression,
                      compression_level != -1,
                      compression_level};
  auto project2 = project.copy(cs);

  // Write project
  project2.write(outputfilename);

  cout << "Done.\n";
  return 0;
}
