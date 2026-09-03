#include <asdf/asdf.hxx>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace ASDF {

// ASDF

asdf::asdf(const shared_ptr<reader_state> &rs, const YAML::Node &node,
           const map<string, reader_t> &readers) {
  const auto &tag = node.Tag();
  ASDF_CHECK(classify_core_tag(tag) == core_tag_t::asdf,
             "Unknown root tag \"" + tag +
                 "\"; expected core/asdf-1.0.0 or core/asdf-1.1.0");

  input_header = rs->get_input_header();

  ASDF_CHECK(readers.empty(), "Custom readers are not supported");
  // if (readers.count(tag))
  //   readers.at(tag)(rs, key, node);

  // History entries are not supported and are ignored when reading. (Their
  // tagged extension metadata would otherwise be rejected as unknown tags.)
  ASDF_CHECK(node.IsMap(), "The ASDF tree must be a mapping");
  grp = std::make_shared<group>();
  for (const auto &key_value : node) {
    const auto key = key_value.first.Scalar();
    if (key == "history")
      continue;
    grp->insert(key, make_entry(rs, key_value.second));
  }
}

asdf::asdf(const copy_state &cs, const asdf &project)
    : input_header(project.input_header) {
  if (project.grp)
    grp = make_shared<group>(cs, *project.grp);
}

writer &asdf::to_yaml(writer &w) const {
  w << YAML::LocalTag(w.standard().asdf_tag);
  w << YAML::BeginMap;
  w << YAML::Key << "asdf_library" << YAML::Value
    << software(ASDF_CXX_NAME, ASDF_CXX_AUTHOR, ASDF_CXX_HOMEPAGE,
                ASDF_CXX_VERSION);
  if (grp)
    for (const auto &[key, value] : *grp->get_group())
      if (key != "asdf_library")
        w << YAML::Key << key << YAML::Value << *value;
  for (const auto &kv : nodes)
    w << YAML::Key << kv.first << YAML::Value << kv.second;
  for (const auto &kv : writers) {
    w << YAML::Key << kv.first << YAML::Value;
    kv.second(w);
  }
  w << YAML::EndMap;
  return w;
}

YAML::Node asdf::from_yaml(istream &is) {
  file_header header;
  return from_yaml(is, header);
}

YAML::Node asdf::from_yaml(istream &is, file_header &header) {
  header = file_header();
  ostringstream doc;
  const array<unsigned char, 5> magic{'#', 'A', 'S', 'D', 'F'};
  array<unsigned char, 5> head;
  is.read(reinterpret_cast<char *>(head.data()), head.size());
  if (!is || head != magic) {
    ostringstream msg;
    msg << "This is not an ASDF file";
    if (is) {
      msg << ": the file header should be \"#ASDF\"; found instead \"";
      for (auto ch : head)
        if (ch == '\\' || ch == '"')
          msg << '\\' << ch;
        else if (isprint(ch))
          msg << ch;
        else
          msg << '\\' << oct << setw(3) << setfill('0') << int(ch);
      msg << "\"";
    }
    ASDF_ERROR(msg.str());
  }
  for (auto ch : magic)
    doc << ch;

  // Record, but never reject, the declared format and standard versions
  const auto record = [&header](const string &line) {
    const auto value = [&line](size_t prefix) {
      size_t begin = line.find_first_not_of(" \t", prefix);
      if (begin == string::npos)
        return string();
      size_t end = line.find_last_not_of(" \t\r");
      return line.substr(begin, end + 1 - begin);
    };
    if (line.compare(0, 5, "#ASDF") == 0 && header.asdf_version.empty() &&
        (line.size() == 5 || line[5] == ' ' || line[5] == '\t'))
      header.asdf_version = value(5);
    else if (line.compare(0, 15, "#ASDF_STANDARD ") == 0)
      header.standard_version = value(15);
  };

  // TODO: stream the file instead
  bool first_line = true;
  while (is) {
    string line;
    getline(is, line);
    if (first_line) {
      // The magic has already been consumed; put it back for the parser
      record("#ASDF" + line);
      first_line = false;
    } else {
      record(line);
    }
    doc << line << "\n";
    if (line == "...")
      return YAML::Load(doc.str());
  }
  ASDF_ERROR("Unexpected end of file while reading the YAML tree (missing "
             "\"...\" terminator)");
}

asdf::asdf(const shared_ptr<istream> &pis, const string &filename,
           const map<string, reader_t> &readers) {
  file_header header;
  auto node = from_yaml(*pis, header);
  auto rs = make_shared<reader_state>(node, pis, filename, header);
  *this = asdf(rs, node, readers);
}

asdf::asdf(const string &filename, const map<string, reader_t> &readers)
    : asdf(make_shared<ifstream>(filename, ios::binary | ios::in), filename,
           readers) {}

asdf asdf::copy(const copy_state &cs) const { return asdf(cs, *this); }

content_requirements asdf::requirements() const {
  content_requirements req;
  if (grp)
    grp->collect_requirements(req, "");
  return req;
}

version_t
asdf::resolve_standard_version(const write_options &options,
                               const content_requirements &req) const {
  switch (options.version_mode) {
  case write_options::version_mode_t::minimal:
    return std::max(default_standard_version(), req.minimum_version());
  case write_options::version_mode_t::latest:
    return latest_standard_version();
  case write_options::version_mode_t::input: {
    // Preserve the input file's declared version when this library knows it;
    // fall back to "minimal" for a file that declares none or an unknown one
    if (!input_header.standard_version.empty()) {
      try {
        const version_t version =
            version_t::parse(input_header.standard_version);
        standard_info(version);
        return version;
      } catch (const error &) {
        // fall through
      }
    }
    return std::max(default_standard_version(), req.minimum_version());
  }
  case write_options::version_mode_t::explicit_version:
    return options.explicit_version;
  }
  ASDF_ERROR("Unknown standard version mode");
}

const standard_info_t &asdf::prepare_write(const write_options &options) const {
  const content_requirements req = requirements();
  const version_t version = resolve_standard_version(options, req);
  const standard_info_t &standard = standard_info(version);

  if (!options.allow_nonstandard) {
    if (!req.nonstandard.empty()) {
      ostringstream buf;
      buf << "This tree holds nonstandard content that no version of the "
             "ASDF standard describes:";
      for (const auto &item : req.nonstandard)
        buf << "\n  " << item;
      buf << "\nPass --allow-nonstandard (write_options::allow_nonstandard) "
             "to write it anyway.";
      ASDF_ERROR(buf.str());
    }
    const version_t minimum = req.minimum_version();
    ASDF_CHECK(version >= minimum, "This tree requires ASDF standard version " +
                                       minimum.str() +
                                       ", but standard version " +
                                       version.str() + " was requested");
  }
  return standard;
}

void asdf::write(ostream &os, const write_options &options) const {
  const standard_info_t &standard = prepare_write(options);
  writer w(os, tags, standard, options.allow_nonstandard);
  w << *this;
  w.flush();
}

void asdf::write(const string &filename, const write_options &options) const {
  // Check before opening the output file, so that a refused write does not
  // truncate it
  prepare_write(options);
  ofstream os(filename, ios::binary | ios::trunc | ios::out);
  write(os, options);
}

} // namespace ASDF
