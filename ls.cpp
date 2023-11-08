#include "asdf.hpp"

#include <cassert>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using namespace ASDF;
using namespace std;

const int indent_step = 2;

void output(std::ostream &os, const int indent,
            const std::shared_ptr<ndarray> &arr);
void output(std::ostream &os, const int indent,
            const std::shared_ptr<reference> &ref);
void output(std::ostream &os, const int indent,
            const std::shared_ptr<group> &grp);
void output(std::ostream &os, const int indent,
            const std::shared_ptr<asdf> &project);

void output(std::ostream &os, const int indent,
            const std::shared_ptr<ndarray> &arr) {
  const auto block_info = *arr->get_block_info();
  os << std::string(indent, ' ') << "block_info:\n";
  os << std::string(indent + indent_step, ' ')
     << "compressor:        " << block_info.compression << "\n";
  os << std::string(indent + indent_step, ' ')
     << "uncompressed size: " << block_info.data_space << "\n";
  os << std::string(indent + indent_step, ' ')
     << "compressed size:   " << block_info.used_space << "\n";
  using std::lrint;
  os << std::string(indent + indent_step, ' ') << "compression ratio: "
     << floor(1000.0 * block_info.used_space / block_info.data_space) / 10
     << "%\n";
  os << std::string(indent + indent_step, ' ') << "checksum: ";
  for (const unsigned char ch : block_info.checksum)
    os << std::hex << std::setw(2) << std::setfill('0') << int(ch) << std::dec;
  os << "\n";
}

void output(std::ostream &os, const int indent,
            const std::shared_ptr<reference> &ref) {
  os << std::string(indent, ' ') << "reference:\n";
  os << std::string(indent + indent_step, ' ')
     << "target: " << ref->get_target() << "\n";
}

void output(std::ostream &os, const int indent,
            const std::shared_ptr<sequence> &seq) {
  for (const auto &entry : seq->get_entries()) {
    os << std::string(indent, ' ') << "-\n";
    if (entry->get_array())
      output(os, indent + indent_step, entry->get_array());
    if (entry->get_reference())
      output(os, indent + indent_step, entry->get_reference());
    if (entry->get_sequence())
      output(os, indent + indent_step, entry->get_sequence());
    if (entry->get_group())
      output(os, indent + indent_step, entry->get_group());
  }
}

void output(std::ostream &os, const int indent,
            const std::shared_ptr<group> &grp) {
  for (const auto &[name, entry] : grp->get_entries()) {
    os << std::string(indent, ' ') << name << ":\n";
    if (entry->get_array())
      output(os, indent + indent_step, entry->get_array());
    if (entry->get_reference())
      output(os, indent + indent_step, entry->get_reference());
    if (entry->get_sequence())
      output(os, indent + indent_step, entry->get_sequence());
    if (entry->get_group())
      output(os, indent + indent_step, entry->get_group());
  }
}

void output(std::ostream &os, const int indent,
            const std::shared_ptr<asdf> &project) {
  os << "Project:\n";
  output(os, indent + indent_step, project->get_group());
}

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

    // Output block info
    const auto project = std::make_shared<asdf>(filename);
    output(std::cout, 0, project);
  }

  cout << "Done.\n";
  return 0;
}
