// database.h
//
// database types, and high level i/o functions

#pragma once

#include <stdexcept>
#include <set>
#include <map>
#include <vector>
#include <ctime>

#include <boost/iostreams/filtering_stream.hpp>

using istream = boost::iostreams::filtering_istream;

struct db_format_exception : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

std::string istream_line(const istream &);

using strset = std::set<std::string>;

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
  strset types;
  strset perms;
  strset negate_perms;
};

struct attrib {
  std::string name;
  dbref creator = -1;
  strset flags;
  std::string data;
  int derefs = 0;
};

struct lock {
  std::string type;
  dbref creator = -1;
  strset flags;
  int derefs = 0;
  std::string key;
};

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
  std::map<std::string, lock> locks;
  strset flags;
  strset powers;
  strset warnings;
  std::map<std::string, attrib> attribs;
};

struct database {
  std::string saved_time;
  std::map<std::string, flag> flags;
  std::map<std::string, flag> powers;
  std::map<std::string, attrib> attribs;
  std::vector<dbthing> objects;
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

enum class COMP { NONE, Z, GZ, BZ2 };

extern bool verbose;

database read_database(const std::string &, COMP = COMP::NONE, bool = false);
void write_database(const database &, const std::string &, COMP = COMP::NONE);

istream &operator>>(istream &, database &);
std::ostream &operator<<(std::ostream &, const database &);
