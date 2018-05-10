#include <yaml-cpp/yaml.h>

#include <iostream>

using namespace std;

int main(int argc, char **argv) {
  cout << "asdf-ls:\n";
  YAML::Node node = YAML::LoadFile("demo.asdf");
  cout << node << "\n";
  cout << "Done.\n";
  return 0;
}
