#include <iostream>
#include <string>
#include <unistd.h>

#include "database.h"

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
print_color(const dbthing &obj) {
  auto color = obj.attribs.find("COLOR");
  if (color != obj.attribs.end()) {
    std::cout << ", color=" << color->second.data;
  }
}

int
main(int argc, char **argv)
{
  COMP comp{};
  std::string dbfile = "-";
  int opt;

  while ((opt = getopt(argc, argv, "zZj")) != -1) {
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
    default:
      std::cerr << "Usage: " << argv[0] << " [-z] [FILENAME]\n";
      return EXIT_FAILURE;
    }
  }

  if (optind < argc) {
    dbfile = argv[optind];
  }

  try {
    auto db = read_database(dbfile, comp);

    std::cout << "digraph world {\n"
              << "\tnode [style=filled]\n"
              << "\tedge [len=1]\n";
    
    for (const auto &obj : db.objects) {
      if (obj.type == dbtype::ROOM) {
        std::cout << "\troom" << obj.num;
        std::cout << " [label=\"" << obj.name << "\\n#" << obj.num << "\"";
        print_color(obj);
        std::cout << "]\n";
      } else if (obj.type == dbtype::EXIT) {
        if (obj.location >= 0) {
          std::cout << "\troom" << obj.exits << " -> room" << obj.location;
          std::cout << " [label=\"" << first(obj.name, ';') << "\\n#" << obj.num
                    << '"';
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
          } catch (std::logic_error &e) {
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
