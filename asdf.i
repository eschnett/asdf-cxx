// SWIG interface file

%module asdf

%{
  #include "asdf.hpp"
  using namespace ASDF;
%}

%include <std_complex.i>
%include <std_map.i>
%include <std_shared_ptr.i>
%include <std_string.i>
%include <std_vector.i>

using std::string;

%shared_ptr(asdf);
%shared_ptr(datatype_t);
%shared_ptr(entry);
%shared_ptr(field_t);
%shared_ptr(group);
%shared_ptr(ndarray);
%shared_ptr(reference);
%shared_ptr(sequence);

%template(map_string_string)
  std::map<string, string>;
%template(map_string_shared_ptr_entry)
  std::map<string, std::shared_ptr<entry>>;
%template(map_string_shared_ptr_ndarray)
  std::map<string, std::shared_ptr<ndarray>>;

%template(vector_bool) std::vector<bool>;
%template(vector_complex_double) std::vector<std::complex<double>>;
%template(vector_complex_float) std::vector<std::complex<float>>;
%template(vector_double) std::vector<double>;
%template(vector_float) std::vector<float>;
%template(vector_int) std::vector<int>;
%template(vector_long_long) std::vector<long long>;
%template(vector_shared_ptr_entry) std::vector<std::shared_ptr<entry>>;
%template(vector_short) std::vector<short>;
%template(vector_signed_char) std::vector<signed char>;
%template(vector_string) std::vector<string>;
%template(vector_unsigned_char) std::vector<unsigned char>;
%template(vector_unsigned_int) std::vector<unsigned int>;
%template(vector_unsigned_long_long) std::vector<unsigned long long>;
%template(vector_unsigned_short) std::vector<unsigned short>;

%nodefaultctor;



enum class block_format_t { undefined, block, inline_array };
enum class compression_t { undefined, none, bzip2, zlib };



%{
struct reader_state_node {
  std::shared_ptr<reader_state> rs;
  YAML::Node node;
};
%}
struct reader_state_node {
};



enum scalar_type_id_t {
  id_bool8,
  id_int8,
  id_int16,
  id_int32,
  id_int64,
  id_uint8,
  id_uint16,
  id_uint32,
  id_uint64,
  id_float32,
  id_float64,
  id_complex64,
  id_complex128,
  id_ascii,
  id_ucs4,
};

%{
  constexpr scalar_type_id_t get_scalar_type_id_int32() { return get_scalar_type_id<int32_t>(); }
  constexpr scalar_type_id_t get_scalar_type_id_int64() { return get_scalar_type_id<int64_t>(); }
  constexpr scalar_type_id_t get_scalar_type_id_float32() { return get_scalar_type_id<float32_t>(); }
  constexpr scalar_type_id_t get_scalar_type_id_float64() { return get_scalar_type_id<float64_t>(); }
  constexpr scalar_type_id_t get_scalar_type_id_complex64() { return get_scalar_type_id<complex64_t>(); }
  constexpr scalar_type_id_t get_scalar_type_id_complex128() { return get_scalar_type_id<complex128_t>(); }
%}
constexpr scalar_type_id_t get_scalar_type_id_int32();
constexpr scalar_type_id_t get_scalar_type_id_int64();
constexpr scalar_type_id_t get_scalar_type_id_float32();
constexpr scalar_type_id_t get_scalar_type_id_float64();
constexpr scalar_type_id_t get_scalar_type_id_complex64();
constexpr scalar_type_id_t get_scalar_type_id_complex128();

class datatype_t;

class field_t {
 public:
};

class datatype_t {
 public:
  bool is_scalar;
  scalar_type_id_t scalar_type_id;
  // std::vector<std::shared_ptr<field_t>> fields;
};



class ndarray {
 public:

  %extend {
    // TODO: accept bool
    static std::shared_ptr<ndarray>
      create_bool(const std::vector<long long>& data1,
                  block_format_t block_format,
                  compression_t compression,
                  int compression_level,
                  std::vector<bool> mask,
                  std::vector<long long> shape)
    {
      // TODO: Avoid this copy
      std::vector<bool> data(data1.size());
      for (size_t i=0; i<data.size(); ++i)
        data[i] = data1[i];
      return std::make_shared<ndarray>
        (std::move(data), block_format, compression, compression_level,
         std::move(mask), std::move(shape));
    }
    static std::shared_ptr<ndarray>
      create_int64(std::vector<long long> data,
                   block_format_t block_format,
                   compression_t compression,
                   int compression_level,
                   std::vector<bool> mask,
                   std::vector<long long> shape)
    {
      return std::make_shared<ndarray>
        (std::move(data), block_format, compression, compression_level,
         std::move(mask), std::move(shape));
    }
    static std::shared_ptr<ndarray>
      create_float64(std::vector<double> data,
                     block_format_t block_format,
                     compression_t compression,
                     int compression_level,
                     std::vector<bool> mask,
                     std::vector<long long> shape)
    {
      return std::make_shared<ndarray>
        (std::move(data), block_format, compression, compression_level,
         std::move(mask), std::move(shape));
    }
    static std::shared_ptr<ndarray>
      create_complex128(std::vector<std::complex<double>> data,
                        block_format_t block_format,
                        compression_t compression,
                        int compression_level,
                        std::vector<bool> mask,
                        std::vector<long long> shape)
    {
      return std::make_shared<ndarray>
        (std::move(data), block_format, compression, compression_level,
         std::move(mask), std::move(shape));
    }

    static std::shared_ptr<ndarray>
      read(const reader_state_node &&rs_node)
    {
      return std::make_shared<ndarray>(rs_node.rs, rs_node.node);
    }
  }

  %extend {
    std::vector<signed char> get_data_vector_int8() const
    {
      static_assert(sizeof(signed char) == sizeof(int8_t), "");
      return self->get_data_vector<signed char>();
    }
    std::vector<short> get_data_vector_int16() const
    {
      static_assert(sizeof(short) == sizeof(int16_t), "");
      return self->get_data_vector<short>();
    }
    std::vector<int> get_data_vector_int32() const
    {
      static_assert(sizeof(int) == sizeof(int32_t), "");
      return self->get_data_vector<int>();
    }
    std::vector<long long> get_data_vector_int64() const
    {
      static_assert(sizeof(long long) == sizeof(int64_t), "");
      return self->get_data_vector<long long>();
    }
    std::vector<unsigned char> get_data_vector_uint8() const
    {
      static_assert(sizeof(unsigned char) == sizeof(uint8_t), "");
      return self->get_data_vector<unsigned char>();
    }
    std::vector<unsigned short> get_data_vector_uint16() const
    {
      static_assert(sizeof(unsigned short) == sizeof(uint16_t), "");
      return self->get_data_vector<unsigned short>();
    }
    std::vector<unsigned int> get_data_vector_uint32() const
    {
      static_assert(sizeof(unsigned int) == sizeof(uint32_t), "");
      return self->get_data_vector<unsigned int>();
    }
    std::vector<unsigned long long> get_data_vector_uint64() const
    {
      static_assert(sizeof(unsigned long long) == sizeof(uint64_t), "");
      return self->get_data_vector<unsigned long long>();
    }
    std::vector<float> get_data_vector_float32() const
    {
      static_assert(sizeof(float) == sizeof(float32_t), "");
      return self->get_data_vector<float>();
    }
    std::vector<double> get_data_vector_float64() const
    {
      static_assert(sizeof(double) == sizeof(float64_t), "");
      return self->get_data_vector<double>();
    }
    std::vector<std::complex<float>> get_data_vector_complex64() const
    {
      static_assert(sizeof(std::complex<float>) == sizeof(complex64_t), "");
      return self->get_data_vector<std::complex<float>>();
    }
    std::vector<std::complex<double>> get_data_vector_complex128() const
    {
      static_assert(sizeof(std::complex<double>) == sizeof(complex128_t), "");
      return self->get_data_vector<std::complex<double>>();
    }
  }

  std::shared_ptr<datatype_t> get_datatype() const;
  std::vector<long long> get_shape() const;
  int64_t get_offset() const;
  std::vector<long long> get_strides() const;
};



class reference {
 public:

  %extend {
    static std::shared_ptr<reference>
      create_from_target(string target)
    {
      return std::make_shared<reference>(std::move(target));
    }
    static std::shared_ptr<reference>
      create_from_path(const string &base_target,
                       const std::vector<string> &doc_path)
    {
      return std::make_shared<reference>(base_target, doc_path);
    }
    static std::shared_ptr<reference>
      create_from_reader_state_node(const reader_state_node &rs_node)
    {
      return std::make_shared<reference>(rs_node.rs, rs_node.node);
    }
  }

  string get_target() const;
  std::pair<string, std::vector<string>> get_split_target() const;

  %extend {
    reader_state_node resolve() const
    {
      auto rs_node = self->resolve();
      return reader_state_node{std::get<0>(rs_node), std::get<1>(rs_node)};
    }
  }
};



class sequence;
class group;

class entry {
 public:

  %extend {
    static std::shared_ptr<entry>
      create_from_ndarray(const string& name,
                          const std::shared_ptr<ndarray>& arr,
                          const string& description)
    {
      return std::make_shared<entry>(name, arr, description);
    }
    static std::shared_ptr<entry>
      create_from_reference(const string& name,
                            const std::shared_ptr<reference>& ref,
                            const string& description)
    {
      return std::make_shared<entry>(name, ref, description);
    }
    static std::shared_ptr<entry>
      create_from_sequence(const string& name,
                           const std::shared_ptr<sequence>& seq,
                           const string& description)
    {
      return std::make_shared<entry>(name, seq, description);
    }
    static std::shared_ptr<entry>
      create_from_group(const string& name,
                        const std::shared_ptr<group>& grp,
                        const string& description)
    {
      return std::make_shared<entry>(name, grp, description);
    }
  }

  string get_name() const;
  std::shared_ptr<ndarray> get_array() const;
  std::shared_ptr<reference> get_reference() const;
  std::shared_ptr<sequence> get_sequence() const;
  std::shared_ptr<group> get_group() const;
  string get_description() const;
};

class sequence {
 public:

  %extend {
    static std::shared_ptr<sequence>
      create(std::vector<std::shared_ptr<entry>> entries)
    {
      return std::make_shared<sequence>(std::move(entries));
    }
  }

  const vector<shared_ptr<entry>> &get_entries() const;
};

class group {
 public:

  %extend {
    static std::shared_ptr<group>
      create(std::map<string, std::shared_ptr<entry>> entries)
    {
      return std::make_shared<group>(std::move(entries));
    }
  }

  const std::map<string, std::shared_ptr<entry>> &get_entries() const;
};



class asdf {
 public:

  %extend {
    static std::shared_ptr<asdf>
      create_from_ndarrays(std::map<string, string> tags,
                           std::map<string, std::shared_ptr<ndarray>> data)
    {
      return std::make_shared<asdf>(std::move(tags), std::move(data));
    }
    static std::shared_ptr<asdf>
      create_from_group(std::map<string, string> tags,
                        std::shared_ptr<group> grp)
    {
      return std::make_shared<asdf>(std::move(tags), std::move(grp));
    }
    static std::shared_ptr<asdf>
      read(const string &filename)
    {
      return std::make_shared<asdf>(filename);
    }
  }

  void write(const string &filename) const;

  std::shared_ptr<group> get_group() const;
};
