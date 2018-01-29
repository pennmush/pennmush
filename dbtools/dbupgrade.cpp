#include <iostream>
#include <string>

#include <boost/program_options.hpp>

#include "database.h"

using namespace std::literals::string_literals;

int
main(int argc, char **argv)
{
  int comp{COMP::NONE};
  bool inplace{false};

  namespace po = boost::program_options;
  po::options_description desc("Options");
  desc.add_options()("help,h", "print help message")(
    ",z", po::value<int>(&comp)->implicit_value(COMP::GZ, ""s)->zero_tokens(),
    "compressed with gzip")(
    ",j", po::value<int>(&comp)->implicit_value(COMP::BZ2, ""s)->zero_tokens(),
    "compressed with bzip2")(",i", po::bool_switch(&inplace),
                             "update database in place");
  po::options_description hidden("Hidden Options");
  hidden.add_options()("input-file", po::value<std::string>(), "input file");
  po::positional_options_description p;
  p.add("input-file", 1);
  po::options_description allopts;
  allopts.add(desc).add(hidden);
  po::variables_map vm;

  try {
    po::store(
      po::command_line_parser(argc, argv).options(allopts).positional(p).run(),
      vm);
    po::notify(vm);

    if (vm.count("help")) {
      std::cout << "Usage: " << argv[0] << " [OPTIONS] [FILE]\n\n"
                << "Upgrade a Penn DB to the latest version.\n\n"
                << desc << '\n';
      return 0;
    }

    std::string input_db = "-";

    if (vm.count("input-file")) {
      input_db = vm["input-file"].as<std::string>();
    }

    auto db = read_database(input_db, static_cast<COMP>(comp), true);
    db.fix_up();

    if (inplace && input_db != "-") {
      write_database(db, input_db, static_cast<COMP>(comp));
    } else {
      write_database(db, "-", static_cast<COMP>(comp));
    }
  } catch (std::exception &e) {
    std::cerr << "Error: " << e.what() << '\n';
    return EXIT_FAILURE;
  }
  return 0;
}
