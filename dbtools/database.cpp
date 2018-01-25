#include <iostream>
#include <string>
#include <tuple>

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/counter.hpp>

#include "database.h"
#include "io_primitives.h"
#include "utils.h"

using namespace std::literals::string_literals;

bool verbose = false;

std::tuple<database, int> read_db_labelsv1(istream &, std::uint32_t);
database read_db_oldstyle(istream &, std::uint32_t);
void write_db_labelsv1(std::ostream &, const database &);

std::string
istream_line(const istream &in)
{
  int line = in.component<0, boost::iostreams::counter>()->lines();
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
fix_up_database(database &db, int dbversion, std::uint32_t flags)
{
  /* Flags that we don't have to do anything special about when
     upgrading:

     DBF_NO_STARTUP_FLAG
     DBF_PANIC
  */

  /* Flags handled in this function:

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
     DBF_AF_VISUAL 
     DBF_SPIFFY_LOCKS
     DBF_NEW_FLAGS
     DBF_NEW_POWERS
     DBF_LABELS
     DBF_NEW_VERSIONS
  */

  if (!(flags & DBF_POWERS_LOGGED)) {
    for (auto &p : db.powers) {
      p.second.perms.insert("log");
    }
  }
  
  if (dbversion < 2) {
    db.attribs.insert(
      {"MONIKER",
       {"MONIKER", 0, split_words("no_command wizard visual locked"), "", 0}});
  }

  if (dbversion < 4) {
    db.flags["HAVEN"].types.erase("ROOM");
  }

  if (dbversion < 5) {
    db.attribs.insert(
      {"MAILQUOTA",
       {"MAILQUOTA", 0, split_words("no_command no_clone wizard locked"), "",
        0}});
  }

  if (dbversion < 6) {
    db.powers.erase("Cemit");
    db.powers.erase("@cemit");
  }
  
  for (auto &obj : db.objects) {

    obj.flags.erase("GOING");
    obj.flags.erase("GOING_TWICE");

    if (!(flags & DBF_AF_NODUMP)) {
      obj.attribs.erase("QUEUE");
      obj.attribs.erase("SEMAPHORE");
    }

    if (dbversion < 6) {
      obj.powers.erase("Cemit");
    }
    
    switch (obj.type) {
    case dbtype::THING:
      if (!(flags & DBF_VALUE_IS_COST)) {
        obj.pennies = (obj.pennies + 1) * 5;
      }
      break;
      
    case dbtype::PLAYER:
      obj.flags.erase("CONNECTED");
      if (!(flags & DBF_HEAR_CONNECT && obj.flags.count("MONITOR"))) {
        obj.flags.erase("MONITOR");
        obj.flags.insert("HEAR_CONNECT");
      }
      break;
      
    case dbtype::ROOM:
      if (dbversion < 4) {
        obj.flags.erase("HAVEN");
      }
      break;
      
    case dbtype::EXIT:
      if (obj.location == AMBIGUOUS && !(flags & DBF_LINK_ANYWHERE)) {
        obj.powers.insert("LINK_ANYWHERE");
      }
      break;

    default:
      break;
    }
  }
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
  {DBF_NO_STARTUP_FLAG, "no-start-up-flag"},
  {DBF_PANIC, "PANIC"},
  {DBF_AF_NODUMP, "af_nodump"},
  {DBF_SPIFFY_LOCKS, "spiffy-locks"},
  {DBF_NEW_FLAGS, "new-flags"},
  {DBF_NEW_POWERS, "new-powers"},
  {DBF_LABELS, "labels"},
  {DBF_SPIFFY_AF_ANSI, "spiffy-af_ansi"},
  {DBF_HEAR_CONNECT, "hear_connect"},
  {DBF_NEW_VERSIONS, "new-versions"},
  {0, nullptr}
};

std::string
dbflags_to_str(std::uint32_t bits) {
  strset flags;
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
  std::uint32_t minimum_flags =
    DBF_NEW_STRINGS | DBF_TYPE_GARBAGE | DBF_SPLIT_IMMORTAL | DBF_NO_TEMPLE;

  if (verbose) {
    std::cerr << "Present database flags: " << dbflags_to_str(flags) << '\n';
  }
  
  if ((flags & minimum_flags) != minimum_flags) {
    throw db_format_exception{"Unable to read this database version. Minimum flags: "s + dbflags_to_str(minimum_flags)};
  }
  
  int dbversion = 0;
  if (flags & DBF_LABELS) {
    std::tie(db, dbversion) = read_db_labelsv1(in, flags);
    if (verbose) {
      std::cerr << "Database version " << dbversion << '\n';
    }
  } else {
    db = read_db_oldstyle(in, flags);
  }
  fix_up_database(db, dbversion, flags);
  return in;
}

database
read_database(const std::string &name, COMP compress_type, bool vrbse)
{
  namespace io = boost::iostreams;
  database db;

  verbose = vrbse;
  
  istream dbin;
  dbin.push(io::counter{});
  
  switch (compress_type) {
  case COMP::Z:
    dbin.push(io::zlib_decompressor{});
    break;
  case COMP::GZ:
    dbin.push(io::gzip_decompressor{});
    break;
  case COMP::BZ2:
    dbin.push(io::bzip2_decompressor{});
    break;
  default:
    break;
  }
  
  if (name == "-") {
    dbin.push(std::cin);
  } else {
    dbin.push(io::file_source{name});
  }

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
  case COMP::Z:
    dbout.push(io::zlib_compressor{});
    break;
  case COMP::GZ:
    dbout.push(io::gzip_compressor{});
    break;
  case COMP::BZ2:
    dbout.push(io::bzip2_compressor{});
    break;
  default:
    break;
  }
  
  if (name == "-") {
    dbout.push(std::cout);
  } else {
    dbout.push(io::file_sink{name});
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
