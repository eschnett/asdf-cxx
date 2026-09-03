#ifndef ASDF_ASDF_HXX
#define ASDF_ASDF_HXX

#include <asdf/byteorder.hxx>
#include <asdf/config.hxx>
#include <asdf/datatype.hxx>
#include <asdf/entry.hxx>
#include <asdf/error.hxx>
#include <asdf/io.hxx>
#include <asdf/ndarray.hxx>
#include <asdf/reference.hxx>
#include <asdf/stl.hxx>

#include <yaml-cpp/yaml.h>

#include <map>
#include <memory>

namespace ASDF {
using namespace std;

// ASDF

class asdf {
  // For writing
  map<string, string> tags; // tag directives

  // Content
  shared_ptr<group> grp;

  map<string, YAML::Node> nodes;
  map<string, function<void(writer &w)>> writers;

public:
  asdf() = delete;
  asdf(const asdf &) = default;
  asdf(asdf &&) = default;
  asdf &operator=(const asdf &) = default;
  asdf &operator=(asdf &&) = default;

  asdf(map<string, string> tags1, shared_ptr<group> grp1)
      : tags(std::move(tags1)), grp(std::move(grp1)) {
    ASDF_CHECK(grp, "The root group must not be null");
  }
  asdf(map<string, string> tags1, map<string, YAML::Node> nodes1)
      : tags(std::move(tags1)), nodes(std::move(nodes1)) {}
  asdf(map<string, string> tags1,
       map<string, function<void(writer &w)>> writers1)
      : tags(std::move(tags1)), writers(std::move(writers1)) {}

  typedef function<void(const shared_ptr<reader_state> &rs, const string &name,
                        const YAML::Node &node)>
      reader_t;
  asdf(const shared_ptr<reader_state> &rs, const YAML::Node &node,
       const map<string, reader_t> &readers = {});
  asdf(const copy_state &cs, const asdf &project);
  writer &to_yaml(writer &w) const;
  friend writer &operator<<(writer &w, const asdf &proj) {
    return proj.to_yaml(w);
  }

  static YAML::Node from_yaml(istream &is);
  asdf(const shared_ptr<istream> &pis, const string &filename = {},
       const map<string, reader_t> &readers = {});
  asdf(const string &filename, const map<string, reader_t> &readers = {});
  asdf copy(const copy_state &cs) const;
  void write(ostream &os) const;
  void write(const string &filename) const;

  shared_ptr<group> get_group() const { return grp; }
};

} // namespace ASDF

#define ASDF_ASDF_HXX_DONE
#endif // #ifndef ASDF_ASDF_HXX
#ifndef ASDF_ASDF_HXX_DONE
#error "Cyclic include depencency"
#endif
