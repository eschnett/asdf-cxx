#include "asdf.hpp"

#include <fstream>
#include <iostream>

using namespace std;

int main(int argc, char **argv) {
  cout << "asdf-demo:\n";
  ASDF::asdf objs;
  fstream fout("demo.asdf", ios::binary | ios::trunc | ios::out);
  ASDF::write_asdf(fout, objs);
  fout.close();
  cout << "Done.\n";
  return 0;
}
