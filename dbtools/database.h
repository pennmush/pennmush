// database.h
//
// database types, and high level i/o functions

#pragma once

#include <stdexcept>
#include <vector>
#include <ctime>
#include <boost/iostreams/filtering_stream.hpp>

#include "db_config.h"

#ifdef HAVE_BOOST_CONTAINERS
#include <boost/container/flat_set.hpp>
#include <boost/container/flat_map.hpp>
using stringset = boost::container::flat_set<std::string>;
#else
#include <set>
#include <map>
using stringset = std::set<std::string>;
#endif

using stringvec = std::vector<std::string>;
using istream = boost::iostreams::filtering_istream;

struct db_format_exception : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

std::string istream_line(const istream &);

using dbref = int;
#undef NOTHING
#undef AMBIGUOUS
constexpr dbref NOTHING = -1;
constexpr dbref AMBIGUOUS = -2;

enum class dbtype { ROOM, EXIT, THING, PLAYER, GARBAGE };
dbtype dbtype_from_num(int);
int dbtype_to_num(dbtype);

struct flag {
  std::string name;
  char letter = '\0';
  stringset types;
  stringset perms;
  stringset negate_perms;
};

struct attrib {
  std::string name;
  dbref creator = 0;
  stringvec flags;
  int derefs = 0;
  std::string data;
  attrib() {}
  attrib(std::string n, stringvec f) : name(std::move(n)), flags(std::move(f))
  {
  }
};

struct lock {
  std::string type;
  dbref creator = 0;
  stringvec flags;
  int derefs = 0;
  std::string key;
};

#ifdef HAVE_BOOST_CONTAINERS
using flagmap = boost::container::flat_map<std::string, flag>;
using attrmap = boost::container::flat_map<std::string, attrib>;
using lockmap = boost::container::flat_map<std::string, lock>;
#else
using flagmap = std::map<std::string, flag>;
using attrmap = std::map<std::string, attrib>;
using lockmap = std::map<std::string, lock>;
#endif

struct dbthing {
  dbref num;
  std::string name = "Garbage";
  dbref location = -1;
  dbref contents = -1;
  dbref exits = -1;
  dbref next = -1;
  dbref parent = -1;
  dbref owner = 1;
  dbref zone = -1;
  int pennies = 0;
  dbtype type = dbtype::GARBAGE;
  std::time_t created;
  std::time_t modified;
  lockmap locks;
  stringset flags;
  stringset powers;
  stringvec warnings;
  attrmap attribs;
};

struct database {
  int version = 1;
  std::uint32_t dbflags = 0;
  std::string saved_time;
  bool spiffy_af_ansi = false;
  flagmap flags;
  flagmap powers;
  attrmap attribs;
  std::vector<dbthing> objects;
  void fix_up();
};

/* DB flag macros - these should be defined whether or not the
 * corresponding system option is defined
 * They are successive binary numbers
 */
#define DBF_NO_CHAT_SYSTEM 0x01
#define DBF_WARNINGS 0x02
#define DBF_CREATION_TIMES 0x04
#define DBF_NO_POWERS 0x08
#define DBF_NEW_LOCKS 0x10
#define DBF_NEW_STRINGS 0x20
#define DBF_TYPE_GARBAGE 0x40
#define DBF_SPLIT_IMMORTAL 0x80
#define DBF_NO_TEMPLE 0x100
#define DBF_LESS_GARBAGE 0x200
#define DBF_AF_VISUAL 0x400
#define DBF_VALUE_IS_COST 0x800
#define DBF_LINK_ANYWHERE 0x1000
#define DBF_NO_STARTUP_FLAG 0x2000
#define DBF_PANIC 0x4000
#define DBF_AF_NODUMP 0x8000
#define DBF_SPIFFY_LOCKS 0x10000
#define DBF_NEW_FLAGS 0x20000
#define DBF_NEW_POWERS 0x40000
#define DBF_POWERS_LOGGED 0x80000
#define DBF_LABELS 0x100000
#define DBF_SPIFFY_AF_ANSI 0x200000
#define DBF_HEAR_CONNECT 0x400000
#define DBF_NEW_VERSIONS 0x800000

enum COMP { NONE, GZ, BZ2 };

extern bool verbose;

constexpr int CURRENT_DB_VERSION = 6;

database read_database(const std::string &, COMP = COMP::NONE, bool = false);
void write_database(const database &, const std::string &, COMP = COMP::NONE);

istream &operator>>(istream &, database &);
std::ostream &operator<<(std::ostream &, const database &);
