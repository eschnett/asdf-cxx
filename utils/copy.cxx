#include <asdf/asdf.hxx>

#include <yaml-cpp/yaml.h>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace ASDF;
using namespace std;

int run(int argc, char **argv) {
  cout << "asdf-copy: Copy the content of an ASDF file\n";
  ASDF_CHECK_VERSION();

  // Parse command line arguments
  auto check = [=](bool cond, const string &msg) {
    if (cond)
      return;
    cerr << argv[0] << ": error: " << msg << "Syntax: " << argv[0]
         << " [--array=(block|inline)] "
            "[--compression=(none|blosc|blosc2|bzip2|lz4|lz4f|libzstd|zlib)] "
            "[--compression-level=[0-9]] "
            "[--standard-version=(minimal|latest|input|X.Y.Z)] "
            "[--allow-nonstandard] <input file> <output file>\n"
         << "Aborting.\n";
    exit(1);
  };
  block_format_t block_format = block_format_t::undefined;
  compression_t compression = compression_t::undefined;
  int compression_level = -1;
  // Preserve the input file's declared standard version by default. An input
  // that declares no version, or one this library does not know, falls back
  // to the lowest version that fits the content.
  write_options options;
  options.version_mode = write_options::version_mode_t::input;
  bool standard_version_set = false;
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
    } else if (opt == "--compression=blosc") {
      check(compression == compression_t::undefined,
            "Compression type already set\n");
      compression = compression_t::blosc;
    } else if (opt == "--compression=blosc2") {
      check(compression == compression_t::undefined,
            "Compression type already set\n");
      compression = compression_t::blosc2;
    } else if (opt == "--compression=bzip2") {
      check(compression == compression_t::undefined,
            "Compression type already set\n");
      compression = compression_t::bzip2;
    } else if (opt == "--compression=lz4") {
      check(compression == compression_t::undefined,
            "Compression type already set\n");
      compression = compression_t::lz4;
    } else if (opt == "--compression=lz4f" || opt == "--compression=liblz4") {
      check(compression == compression_t::undefined,
            "Compression type already set\n");
      compression = compression_t::lz4f;
    } else if (opt == "--compression=libzstd") {
      check(compression == compression_t::undefined,
            "Compression type already set\n");
      compression = compression_t::libzstd;
    } else if (opt == "--compression=zlib") {
      check(compression == compression_t::undefined,
            "Compression type already set\n");
      compression = compression_t::zlib;
    } else if (opt.rfind("--compression-level=", 0) == 0) {
      const string value = opt.substr(strlen("--compression-level="));
      const bool is_number =
          !value.empty() && value.size() <= 3 &&
          value.find_first_not_of("0123456789") == string::npos;
      const int level = is_number ? stoi(value) : -1;
      check(is_number && level >= 0 && level <= 9,
            "Compression level \"" + value +
                "\" is not a number from 0 to 9\n");
      compression_level = level;
    } else if (opt.rfind("--standard-version=", 0) == 0) {
      check(!standard_version_set, "Standard version already set\n");
      standard_version_set = true;
      set_standard_version(options, opt.substr(strlen("--standard-version=")));
    } else if (opt == "--allow-nonstandard") {
      options.allow_nonstandard = true;
    } else {
      check(false, "Unknown option " + opt + "\n");
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
  project2.write(outputfilename, options);

  cout << "Done.\n";
  return 0;
}

int main(int argc, char **argv) {
  try {
    return run(argc, argv);
  } catch (const std::exception &e) {
    std::cerr << argv[0] << ": error: " << e.what() << "\n";
    return 1;
  }
}
