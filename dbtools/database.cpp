#include <iostream>
#include <string>
#include <tuple>
#include <algorithm>

#include "database.h"
#include "io_primitives.h"
#include "utils.h"

#include <boost/iostreams/device/file.hpp>
#ifdef ZLIB_FOUND
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#endif
#ifdef BZIP2_FOUND
#include <boost/iostreams/filter/bzip2.hpp>
#endif
#include <boost/iostreams/filter/counter.hpp>

using namespace std::literals::string_literals;

bool verbose = false;

database read_db_labelsv1(istream &, std::uint32_t);
database read_db_oldstyle(istream &, std::uint32_t);
void write_db_labelsv1(std::ostream &, const database &);

std::string
istream_line(const istream &in)
{
  int line = in.component<boost::iostreams::counter>(0)->lines();
  return " at line "s + std::to_string(line);
}

dbtype
dbtype_from_num(int n)
{
  switch (n) {
  case 0x1:
    return dbtype::ROOM;
  case 0x2:
    return dbtype::THING;
  case 0x4:
    return dbtype::EXIT;
  case 0x8:
    return dbtype::PLAYER;
  case 0x10:
    return dbtype::GARBAGE;
  default:
    throw std::runtime_error{"Unknown type: "s + std::to_string(n)};
  }
}

int
dbtype_to_num(dbtype t)
{
  switch (t) {
  case dbtype::ROOM:
    return 0x1;
  case dbtype::THING:
    return 0x2;
  case dbtype::EXIT:
    return 0x4;
  case dbtype::PLAYER:
    return 0x8;
  case dbtype::GARBAGE:
    return 0x10;
  }
  return 0x10;
}

void
database::fix_up()
{
  /* Flags that we don't have to do anything special about when
     upgrading:

     DBF_NO_TEMPLE
     DBF_NO_STARTUP_FLAG
     DBF_PANIC
  */

  /* Flags handled in this function:

     The absence of both DBF_NEW_LOCKS and DBF_SPIFFY_LOCKS
     DBF_VALUE_IS_COST
     DBF_LINK_ANYWHERE
     DBF_AF_NODUMP
     DBF_HEAR_CONNECT
     DBF_POWERS_LOGGED
  */

  /* Flags handled by reader functions:

     DBF_NO_CHAT_SYSTEM
     DBF_WARNINGS
     DBF_CREATION_TIMES
     DBF_NO_POWERS
     DBF_NEW_LOCKS
     DBF_NEW_STRINGS
     DBF_TYPE_GARBAGE
     DBF_SPLIT_IMMORTAL
     DBF_LESS_GARBAGE
     DBF_AF_VISUAL
     DBF_VALUE_IS_COST
     DBF_SPIFFY_LOCKS
     DBF_NEW_FLAGS
     DBF_NEW_POWERS
     DBF_LABELS
     DBF_NEW_VERSIONS
     DBF_SPIFFY_AF_ANSI - Not set in outputed db unless already present
  */

  bool oldold_locks = (dbflags & (DBF_NEW_LOCKS | DBF_SPIFFY_LOCKS)) == 0;

  if (!(dbflags & DBF_POWERS_LOGGED)) {
    for (auto &p : powers) {
      p.second.perms.insert("log");
    }
  }

  if (version < 2) {
    attribs.emplace(
      "MONIKER",
      attrib("MONIKER", split_words_vec("no_command wizard visual locked")));
  }

  if (version < 4) {
    flags["HAVEN"].types.erase("ROOM");
  }

  if (version < 5) {
    attribs.emplace(
      "MAILQUOTA", attrib("MAILQUOTA", split_words_vec(
                                         "no_command no_clone wizard locked")));
  }

  if (version < 6) {
    powers.erase("Cemit");
    powers.erase("@cemit");
  }

  for (auto &obj : objects) {

    flags.erase("GOING");
    flags.erase("GOING_TWICE");

    if (!(dbflags & DBF_AF_NODUMP)) {
      attribs.erase("QUEUE");
      attribs.erase("SEMAPHORE");
    }

    if (version < 6) {
      powers.erase("Cemit");
    }

    if (oldold_locks) {
      // Pre NEW_LOCKS: Clone enter lock to zone lock on zone objects
      if (obj.zone != NOTHING) {
        auto zlock = objects[obj.zone].locks.find("Zone");
        if (zlock == objects[obj.zone].locks.end()) {
          auto elock = objects[obj.zone].locks.find("Enter");
          if (elock != objects[obj.zone].locks.end()) {
            objects[obj.zone].locks.emplace("Zone", elock->second);
          }
        }
      }
    }

    switch (obj.type) {
    case dbtype::THING:
      if (!(dbflags & DBF_VALUE_IS_COST)) {
        obj.pennies = (obj.pennies + 1) * 5;
      }
      break;

    case dbtype::PLAYER:
      obj.flags.erase("CONNECTED");
      if (!(dbflags & DBF_HEAR_CONNECT && obj.flags.count("MONITOR"))) {
        obj.flags.erase("MONITOR");
        obj.flags.insert("HEAR_CONNECT");
      }

      if (oldold_locks) {
        // Pre NEW_LOCKS: Clone Use lock to Page lock
        auto ulock = obj.locks.find("Use");
        if (ulock != obj.locks.end()) {
          obj.locks.emplace("Page", ulock->second);
        }
        // And clone enter lock to zone on ZMPs
        if (obj.flags.count("SHARED")) {
          auto elock = obj.locks.find("Enter");
          if (elock != obj.locks.end()) {
            obj.locks.emplace("Zone", elock->second);
          }
        }
      }
      break;

    case dbtype::ROOM:
      if (version < 4) {
        obj.flags.erase("HAVEN");
      }

      if (oldold_locks) {
        // Pre NEW_LOCKS: Move enter lock to teleport
        auto elock = obj.locks.find("Enter");
        if (elock != obj.locks.end()) {
          obj.locks.emplace("Teleport", elock->second);
          obj.locks.erase(elock);
        }
      }
      break;

    case dbtype::EXIT:
      if (obj.location == AMBIGUOUS && !(dbflags & DBF_LINK_ANYWHERE)) {
        obj.powers.insert("LINK_ANYWHERE");
      }
      break;

    default:
      break;
    }
  }
  version = CURRENT_DB_VERSION;
}

std::pair<std::uint32_t, const char *> dbflag_table[] = {
  {DBF_NO_CHAT_SYSTEM, "no-chat-system"},
  {DBF_WARNINGS, "warnings"},
  {DBF_CREATION_TIMES, "creation-times"},
  {DBF_NO_POWERS, "no-powers"},
  {DBF_NEW_LOCKS, "new-locks"},
  {DBF_NEW_STRINGS, "new-string"},
  {DBF_TYPE_GARBAGE, "garbage"},
  {DBF_SPLIT_IMMORTAL, "split-immortal"},
  {DBF_NO_TEMPLE, "no-temple"},
  {DBF_LESS_GARBAGE, "less-garbage"},
  {DBF_AF_VISUAL, "af_visual"},
  {DBF_VALUE_IS_COST, "value-is-cost"},
  {DBF_LINK_ANYWHERE, "link-anywhere"},
  {DBF_NO_STARTUP_FLAG, "no-startup-flag"},
  {DBF_PANIC, "PANIC"},
  {DBF_AF_NODUMP, "af_nodump"},
  {DBF_SPIFFY_LOCKS, "spiffy-locks"},
  {DBF_NEW_FLAGS, "new-flags"},
  {DBF_NEW_POWERS, "new-powers"},
  {DBF_LABELS, "labels"},
  {DBF_SPIFFY_AF_ANSI, "spiffy-af_ansi"},
  {DBF_HEAR_CONNECT, "hear_connect"},
  {DBF_NEW_VERSIONS, "new-versions"},
  {0, nullptr}};

std::string
dbflags_to_str(std::uint32_t bits)
{
  stringset flags;
  for (int i = 0; dbflag_table[i].second; i += 1) {
    if (dbflag_table[i].first & bits) {
      flags.insert(dbflag_table[i].second);
    }
  }

  return join_words(flags);
}

istream &
operator>>(istream &in, database &db)
{
  char c1, c2;

  in.get(c1);
  in.get(c2);
  if (!(c1 == '+' && c2 == 'V')) {
    throw db_format_exception{"Invalid database format"};
  }

  std::uint32_t flags = ((db_getref(in) - 2) / 256) - 5;
  std::uint32_t minimum_flags = 0;

  if (verbose) {
    std::cerr << "Present database flags: " << dbflags_to_str(flags) << '\n';
  }

  if ((flags & minimum_flags) != minimum_flags) {
    throw db_format_exception{
      "Unable to read this database version. Minimum flags: "s +
      dbflags_to_str(minimum_flags)};
  }

  if (flags & DBF_LABELS) {
    db = read_db_labelsv1(in, flags);
    if (verbose) {
      std::cerr << "Database version " << db.version << '\n';
    }
  } else {
    db = read_db_oldstyle(in, flags);
  }
  db.dbflags = flags;
  return in;
}

database
read_database(const std::string &name, COMP compress_type, bool vrbse)
{
  namespace io = boost::iostreams;
  database db;

  verbose = vrbse;

  istream dbin;
  dbin.push(io::counter{1});

  switch (compress_type) {
  case COMP::NONE:
    break;
#ifdef ZLIB_FOUND
  case COMP::GZ:
    dbin.push(io::gzip_decompressor{});
    break;
#endif
#ifdef BZIP2_FOUND
  case COMP::BZ2:
    dbin.push(io::bzip2_decompressor{});
    break;
#endif
  default:
    throw std::runtime_error{"Unsupported compression type!"};
    break;
  }

  if (name == "-") {
    dbin.push(std::cin);
  } else {
    if (verbose) {
      std::cerr << "Reading from " << name << '\n';
    }
    dbin.push(io::file_source{name, std::ios_base::in | std::ios_base::binary});
  }

  dbin.exceptions(std::istream::badbit);

  dbin.peek();
  if (!dbin) {
    throw std::runtime_error{"Unable to read database."};
  }

  dbin >> db;
  if (dbin.good() || dbin.eof()) {
    return db;
  } else {
    throw std::runtime_error{"Unable to read database."};
  }
}

void
write_database(const database &db, const std::string &name, COMP compress_type)
{
  namespace io = boost::iostreams;
  io::filtering_ostream dbout;

  switch (compress_type) {
  case COMP::NONE:
    break;
#ifdef ZLIB_FOUND
  case COMP::GZ:
    dbout.push(io::gzip_compressor{});
    break;
#endif
#ifdef BZIP2_FOUND
  case COMP::BZ2:
    dbout.push(io::bzip2_compressor{});
    break;
#endif
  default:
    throw std::runtime_error{"Unsupported compression type!\n"};
    return;
  }

  if (name == "-") {
    dbout.push(std::cout);
  } else {
    dbout.push(io::file_sink{name, std::ios_base::out | std::ios_base::binary});
  }

  if (!dbout) {
    throw std::runtime_error{"Unable to write database!"};
  }

  dbout << db;
}

std::ostream &
operator<<(std::ostream &out, const database &db)
{
  write_db_labelsv1(out, db);
  return out;
}
