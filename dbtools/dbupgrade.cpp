#include <iostream>
#include <string>
#include <unistd.h>

#include "database.h"

int
main(int argc, char **argv)
{
  COMP comp{};
  bool inplace{false};
  int opt;

  while ((opt = getopt(argc, argv, "zZji")) != -1) {
    switch (opt) {
    case 'z':
      comp = COMP::GZ;
      break;
    case 'Z':
      comp = COMP::Z;
      break;
    case 'j':
      comp = COMP::BZ2;
      break;
    case 'i':
      inplace = true;
      break;
    default:
      std::cerr << "Usage: " << argv[0] << " [-z] [[-i] FILENAME]\n";
      return EXIT_FAILURE;
    }
  }

  try {
    std::string input_db = "-";

    if (optind < argc) {
      input_db = argv[optind];
    }

    auto db = read_database(input_db, comp, true);
    if (inplace && input_db != "-") {
      write_database(db, input_db, comp);
    } else {
      write_database(db, "-", comp);
    }
  } catch (std::exception &e) {
    std::cerr << "Error: " << e.what() << '\n';
    return EXIT_FAILURE;
  }
  return 0;
}
