#include <iostream>
#include <string>
#include <regex>
#include <boost/program_options.hpp>

#include "database.h"

using namespace std::literals::string_literals;

struct search_fields {
  bool name = false;
  bool locks = false;
  bool attribs = false;
};

void
grep_db(const database &db, const std::regex &re, search_fields what)
{
  for (const auto &obj : db.objects) {
    bool header = false;

    if (what.name && std::regex_search(obj.name.begin(), obj.name.end(), re)) {
      std::cout << '#' << obj.num << ":\n\tName: " << obj.name << '\n';
      header = true;
    }

    if (what.locks) {
      bool inlock = false;
      for (const auto &l2 : obj.locks) {
        const auto &lock = l2.second;
        if (std::regex_search(lock.key.begin(), lock.key.end(), re)) {
          if (!header) {
            std::cout << '#' << obj.num << ":\n";
            header = true;
          }
          if (!inlock) {
            std::cout << "\tLocks:";
            inlock = true;
          }
          std::cout << ' ' << lock.type;
        }
      }
      if (inlock) {
        std::cout << '\n';
      }
    }

    if (what.attribs) {
      bool inattr = false;
      for (const auto &a2 : obj.attribs) {
        const auto &a = a2.second;
        if (std::regex_search(a.data.begin(), a.data.end(), re)) {
          if (!header) {
            std::cout << '#' << obj.num << ":\n";
            header = true;
          }
          if (!inattr) {
            std::cout << "\tAttributes:";
            inattr = true;
          }
          std::cout << ' ' << a.name;
        }
      }
      if (inattr) {
        std::cout << '\n';
      }
    }
  }
}

int
main(int argc, char **argv)
{
  int comp{COMP::NONE};
  bool insensitive{false}, all{false};
  search_fields what;

  namespace po = boost::program_options;
  po::options_description desc("Options");
  desc.add_options()("help,h", "print help message")(
    "name,n", po::bool_switch(&what.name),
    "Search names")("locks,l", po::bool_switch(&what.locks), "Search locks")(
    "attrs,t", po::bool_switch(&what.attribs),
    "Search attributes")("all,a", po::bool_switch(&all), "Search all fields")(
    ",z", po::value<int>(&comp)->implicit_value(COMP::GZ, "")->zero_tokens(),
    "compressed with gzip")(
    ",j", po::value<int>(&comp)->implicit_value(COMP::BZ2, "")->zero_tokens(),
    "compressed with bzip2")(",i", po::bool_switch(&insensitive),
                             "case-insensitive match");
  po::options_description hidden("Hidden options");
  hidden.add_options()("pattern", po::value<std::string>(),
                       "regex to search for")(
    "input-file", po::value<std::string>(), "input file");
  po::positional_options_description p;
  p.add("pattern", 1);
  p.add("input-file", 2);
  po::options_description allopts;
  allopts.add(desc).add(hidden);
  po::variables_map vm;

  try {
    po::store(
      po::command_line_parser(argc, argv).options(allopts).positional(p).run(),
      vm);
    po::notify(vm);

    if (vm.count("help")) {
      std::cout << "Usage: " << argv[0] << " [OPTIONS] RE [FILE]\n\n"
                << "Shows every place a regex is matched in a db.\n\n"
                << desc << '\n';
      return 0;
    }

    std::string pattern;
    if (vm.count("pattern")) {
      pattern = vm["pattern"].as<std::string>();
    } else {
      std::cerr << "Error: Missing search pattern\n";
      return 1;
    }

    std::string input_db = "-";
    if (vm.count("input-file")) {
      input_db = vm["input-file"].as<std::string>();
    }

    auto db = read_database(input_db, static_cast<COMP>(comp), false);

    auto flags =
      std::regex_constants::ECMAScript | std::regex_constants::optimize;
    if (insensitive) {
      flags |= std::regex_constants::icase;
    }

    if (all ||
        (what.name == false && what.locks == false && what.attribs == false)) {
      what.name = what.locks = what.attribs = true;
    }

    grep_db(db, std::regex{pattern, flags}, what);

  } catch (std::exception &e) {
    std::cerr << "Error: " << e.what() << '\n';
    return 1;
  }
  return 0;
}
