#include <iostream>
#include <string>

#include <boost/program_options.hpp>

#include "database.h"

using namespace std::literals::string_literals;

std::string
escape(const std::string &s)
{
  std::string esc;
  esc.reserve(s.size());

  for (char c : s) {
    if (c == '"' || c == '\\') {
      esc.push_back('\\');
    }
    esc.push_back(c);
  }
  return esc;
}

std::string
first(const std::string &s, char delim)
{
  auto pos = s.find(delim);
  if (pos == std::string::npos) {
    return s;
  } else {
    return s.substr(0, pos);
  }
}

void
print_color(const dbthing &obj)
{
  auto color = obj.attribs.find("COLOR");
  if (color != obj.attribs.end()) {
    std::cout << ", color=" << color->second.data;
  }
}

int
main(int argc, char **argv)
{
  int comp{COMP::NONE};
  std::string dbfile = "-";

  namespace po = boost::program_options;
  po::options_description desc("Options");
  desc.add_options()("help,h", "print help message")(
    ",z", po::value<int>(&comp)->implicit_value(COMP::GZ, "")->zero_tokens(),
    "compressed with gzip")(
    ",j", po::value<int>(&comp)->implicit_value(COMP::BZ2, "")->zero_tokens(),
    "compressed with bzip2");
  po::options_description hidden("Hidden options");
  hidden.add_options()("input-file", po::value<std::string>(), "input file");
  po::positional_options_description p;
  p.add("input-file", 1);
  po::options_description allopts;
  allopts.add(desc).add(hidden);

  try {
    po::variables_map vm;
    po::store(
      po::command_line_parser(argc, argv).options(allopts).positional(p).run(),
      vm);
    po::notify(vm);

    if (vm.count("help")) {
      std::cout << "Usage: " << argv[0] << " [OPTIONS] [FILE]\n\n"
                << "Turn a Penn DB into a graphviz dot file.\n\n"
                << desc << '\n';
      return 0;
    }

    if (vm.count("input-file")) {
      dbfile = vm["input-file"].as<std::string>();
    }

    auto db = read_database(dbfile, static_cast<COMP>(comp));

    std::cout << "digraph world {\n"
              << "\tnode [style=filled]\n"
              << "\tedge [len=1]\n";

    for (const auto &obj : db.objects) {
      if (obj.type == dbtype::ROOM) {
        std::cout << "\troom" << obj.num;
        std::cout << " [label=\"" << escape(obj.name) << "\\n#" << obj.num
                  << "\"";
        print_color(obj);
        std::cout << "]\n";
      } else if (obj.type == dbtype::EXIT) {
        if (obj.location >= 0) {
          std::cout << "\troom" << obj.exits << " -> room" << obj.location;
          std::cout << " [label=\"" << escape(first(obj.name, ';')) << "\\n#"
                    << obj.num << '"';
          if (obj.locks.count("Basic")) {
            std::cout << ", style=dashed";
          } else {
            std::cout << ", style=solid";
          }
          try {
            auto dist = obj.attribs.find("DISTANCE");
            if (dist != obj.attribs.end()) {
              auto d = std::stoi(dist->second.data);
              std::cout << ", len=" << d;
            }
          } catch (std::logic_error &) {
            // DISTANCE didn't hold an integer. Ignore it.
          }
          print_color(obj);
          std::cout << "]\n";
        }
      }
    }
    std::cout << "}\n";
    return 0;
  } catch (std::runtime_error &e) {
    std::cerr << "Error: " << e.what() << '\n';
    return EXIT_FAILURE;
  }
}
