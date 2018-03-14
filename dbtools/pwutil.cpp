#include <iostream>
#include <string>

#include <boost/program_options.hpp>

#include "database.h"
#include "hasher.h"

using namespace std::literals::string_literals;

void
update_password(database &db, dbref who, const std::string &newpass,
                password_hasher *hash)
{
  auto xyxxy = db.objects[who].attribs.find("XYXXY");
  if (xyxxy != db.objects[who].attribs.end()) {
    // Update existing attribute
    xyxxy->second.data = hash->make_password(newpass);
  } else {
    // Add a new attribute if not already present
    attrib newxyxxy = db.attribs["XYXXY"];
    newxyxxy.creator = 1;
    newxyxxy.data = hash->make_password(newpass);
    db.objects[who].attribs.emplace("XYXXY", std::move(newxyxxy));
  }
}

int
main(int argc, char **argv)
{
  dbref who;
  int comp{COMP::NONE};
  bool all = false, clear = false, inplace = false;
  std::string newpass;

  namespace po = boost::program_options;
  po::options_description desc("Options");
  desc.add_options()("help,h", "print help message")(
    ",z", po::value<int>(&comp)->implicit_value(COMP::GZ, "")->zero_tokens(),
    "compressed with gzip")(
    ",j", po::value<int>(&comp)->implicit_value(COMP::BZ2, "")->zero_tokens(),
    "compressed with bzip2")("inplace,i", po::bool_switch(&inplace),
                             "update database in place")(
    "dbref,d", po::value<int>(&who)->default_value(-1),
    "Player to modify")("all,a", po::bool_switch(&all), "Modify all players")(
    "clear,c", po::bool_switch(&clear), "Erase password")(
    "password,p", po::value<std::string>(&newpass)->default_value("hunter2"),
    "New password");
  po::options_description hidden("Hidden options");
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
                << "Edits player passwords in a Penn DB.\n\n"
                << desc << '\n';
      return 0;
    }

    if (who < 0 && !all) {
      std::cerr
        << "Either -a must be given (For all players) or -d DBREF (For a "
           "specific player)\n";
      return EXIT_FAILURE;
    }

    std::string input_db = "-";

    if (vm.count("input-file")) {
      input_db = vm["input-file"].as<std::string>();
    }

    auto db = read_database(input_db, static_cast<COMP>(comp));
    db.fix_up();

    auto hash = make_password_hasher();

    if (who >= 0) {
      if (static_cast<std::size_t>(who) >= db.objects.size()) {
        std::cerr << "Object #" << who << " is out of range!\n";
        return EXIT_FAILURE;
      }
      if (db.objects[who].type != dbtype::PLAYER) {
        std::cerr << "Object #" << who << " is not a player!\n";
        return EXIT_FAILURE;
      }
      if (clear) {
        db.objects[who].attribs.erase("XYXXY");
      } else {
        update_password(db, who, newpass, hash.get());
      }
    } else if (all) {
      for (auto &obj : db.objects) {
        if (obj.type == dbtype::PLAYER) {
          if (clear) {
            obj.attribs.erase("XYXXY");
          } else {
            update_password(db, obj.num, newpass, hash.get());
          }
        }
      }
    }

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
