#include <yaml-cpp/yaml.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

int main(int argc, char **argv) {
  cout << "asdf-ls:\n";
  fstream fin("demo.asdf", ios::binary | ios::in);
  ostringstream doc;
  for (;;) {
    string line;
    getline(fin, line);
    doc << line << "\n";
    if (line == "...")
      break;
  }
  fin.close();
  YAML::Node node = YAML::Load(doc.str());
  cout << node << "\n";
  cout << "Done.\n";
  return 0;
}
