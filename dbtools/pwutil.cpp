#include <iostream>
#include <sstream>
#include <string>
#include <ctime>
#include <random>
#include <unistd.h>
#include <openssl/sha.h>

#include "database.h"

std::string
make_password_string(const std::string &plain)
{
  static const unsigned char salts[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

  static std::random_device rd;
  static std::ranlux48 rng{rd()};
  std::uniform_int_distribution<int> pick(0, sizeof salts - 1);

  unsigned char salt[2];
  salt[0] = salts[pick(rng)];
  salt[1] = salts[pick(rng)];

  SHA512_CTX ctx;
  SHA512_Init(&ctx);
  SHA512_Update(&ctx, salt, 2);
  SHA512_Update(&ctx, reinterpret_cast<const unsigned char *>(plain.c_str()),
                plain.size());
  unsigned char hashed[SHA512_DIGEST_LENGTH];
  SHA512_Final(hashed, &ctx);

  std::ostringstream sink;

  sink << "2:sha512:" << salt[0] << salt[1] << std::hex;
  for (auto byte : hashed) {
    sink << static_cast<unsigned>(byte);
  }
  sink << std::dec << ':' << std::time(nullptr);

  return sink.str();
}

void
update_password(database &db, dbref who, const std::string &newpass)
{
  auto xyxxy = db.objects[who].attribs.find("XYXXY");
  if (xyxxy != db.objects[who].attribs.end()) {
    // Update existing attribute
    xyxxy->second.data = make_password_string(newpass);
  } else {
    // Add a new attribute if not already present
    attrib newxyxxy = db.attribs["XYXXY"];
    newxyxxy.creator = 1;
    newxyxxy.data = make_password_string(newpass);
    db.objects[who].attribs.insert({"XYXXY", std::move(newxyxxy)});
  }
}

int
main(int argc, char **argv)
{
  dbref who = -1;
  COMP comp{};
  bool all = false, clear = false, inplace = false;
  std::string newpass = "hunter2";
  int opt;

  while ((opt = getopt(argc, argv, "d:p:aczZji")) != -1) {
    switch (opt) {
    case 'd':
      who = std::stoi(optarg);
      all = false;
      break;
    case 'p':
      newpass = optarg;
      clear = false;
      break;
    case 'a':
      all = true;
      who = -1;
      break;
    case 'c':
      clear = true;
      break;
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
      std::cerr << "Usage: " << argv[0]
                << " -d DBREF | -a [-p NEWPASSWORD | -c] [-z] [[-i] FILE]\n";
      return EXIT_FAILURE;
    }
  }

  if (who < 0 && !all) {
    std::cerr << "Either -a must be given (For all players) or -d DBREF (For a "
                 "specific player)\n";
    return EXIT_FAILURE;
  }

  try {
    std::string input_db = "-";

    if (optind < argc) {
      input_db = argv[optind];
    }

    auto db = read_database(input_db, comp);

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
        update_password(db, who, newpass);
      }
    } else if (all) {
      for (auto &obj : db.objects) {
        if (obj.type == dbtype::PLAYER) {
          if (clear) {
            obj.attribs.erase("XYXXY");
          } else {
            update_password(db, obj.num, newpass);
          }
        }
      }
    }

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
