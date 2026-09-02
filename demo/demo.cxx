#include <asdf/asdf.hxx>

#include <yaml-cpp/yaml.h>

#include <cassert>
#include <complex>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using namespace ASDF;

int run(int argc, char **argv) {
  cout << "asdf-demo: Create a simple ASDF file\n";
  ASDF_CHECK_VERSION();

  auto grp = make_shared<group>();

  auto array1d = make_shared<ndarray>(
      vector<int64_t>{1, 2, 3}, block_format_t::block, compression_t::none, 0,
      vector<bool>(), vector<int64_t>{3});
  grp->emplace("beta", array1d);

  auto array2d =
      make_shared<ndarray>(vector<float64_t>{1.0, 2.0, 3.0, 4.0, 5.0, 6.0},
                           block_format_t::inline_array, compression_t::none, 0,
                           vector<bool>(), vector<int64_t>{2, 3});
  grp->emplace("gamma", array2d);

  auto array3d = make_shared<ndarray>(
      vector<complex128_t>{{1, 0}, {-2, 0}, {0, 3}, {-4, 0}, {5, 1}, {6, -1}},
      block_format_t::block, compression_t::bzip2, 9, vector<bool>(),
      vector<int64_t>{1, 2, 3});
  grp->emplace("delta", array3d);

  auto array3db = make_shared<ndarray>(
      vector<complex128_t>{{1, 0}, {-2, 0}, {0, 3}, {-4, 0}, {5, 1}, {6, -1}},
      block_format_t::inline_array, compression_t::bzip2, 9, vector<bool>(),
      vector<int64_t>{1, 2, 3});
  grp->emplace("deltab", array3db);

  auto array8d = make_shared<ndarray>(
      vector<bool8_t>{true}, block_format_t::block, compression_t::zlib, 9,
      vector<bool>(), vector<int64_t>{1, 1, 1, 1, 1, 1, 1, 1});
  grp->emplace("epsilon", array8d);

  // A structured (record) datatype with three fields, the last one a
  // sub-array
  auto record_datatype = make_shared<datatype_t>(vector<shared_ptr<field_t>>{
      make_shared<field_t>("x", make_shared<datatype_t>(id_int32), false,
                           byteorder_t::undefined, vector<int64_t>()),
      make_shared<field_t>("y", make_shared<datatype_t>(id_float64), false,
                           byteorder_t::undefined, vector<int64_t>()),
      make_shared<field_t>("v", make_shared<datatype_t>(id_int16), false,
                           byteorder_t::undefined, vector<int64_t>{2})});
  // Pack the records by hand, in host byte order and without padding
  vector<unsigned char> record_data;
  const auto append = [&record_data](const auto &val) {
    const auto *const ptr = reinterpret_cast<const unsigned char *>(&val);
    record_data.insert(record_data.end(), ptr, ptr + sizeof val);
  };
  const int64_t nrecords = 3;
  for (int64_t n = 0; n < nrecords; ++n) {
    append(int32_t(n + 1));
    append(float64_t(1.5 * (n + 1)));
    append(int16_t(10 * n));
    append(int16_t(10 * n + 1));
  }
  assert(record_data.size() == nrecords * record_datatype->type_size());

  auto record_array = make_shared<ndarray>(
      make_constant_memoized(shared_ptr<block_t>(
          make_shared<typed_block_t<unsigned char>>(record_data))),
      optional<block_info_t>(), block_format_t::block, compression_t::none, 0,
      vector<bool>(), record_datatype, host_byteorder(),
      vector<int64_t>{nrecords});
  grp->emplace("theta", record_array);

  auto seq = make_shared<sequence>();
  seq->emplace_back(array1d);
  seq->emplace_back(array2d);
  grp->emplace("zeta", seq);

  auto ref = make_shared<reference>("", vector<string>{"zeta", "1"});
  grp->emplace("eta", ref);

  vector<uint8_t> data4d;
  data4d.reserve(4 * 2 * 16 * 8);
  for (int l = 0; l < 8; ++l)
    for (int k = 0; k < 16; ++k)
      for (int j = 0; j < 2; ++j)
        for (int i = 0; i < 4; ++i)
          data4d.push_back(unsigned(i + 4 * (j + 2 * (k + 16 * l))));
  auto arr4d = make_shared<ndarray>(std::move(data4d), block_format_t::block,
                                    compression_t::zlib, 9, vector<bool>(),
                                    vector<int64_t>{4, 2, 16, 8});
  grp->emplace(
      "attributed",
      make_shared<group>(std::map<string, shared_ptr<entry>>{
          {"null", make_shared<null_entry>()},
          {"fbool", make_shared<bool_entry>(false)},
          {"tbool", make_shared<bool_entry>(true)},
          {"int", make_shared<int_entry>(42)},
          {"float", make_shared<float_entry>(12.3)},
          {"complex", make_shared<complex_entry>(complex64_t(-4.4, -5.5))},
          {"string", make_shared<string_entry>("hello")},
          {"array4d", make_shared<ndarray_entry>(std::move(arr4d))}}));

  auto project = make_shared<asdf>(map<string, string>(), grp);

  project->write("demo.asdf");

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
