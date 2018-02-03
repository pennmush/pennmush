#include <iostream>
#include <string>

#include "database.h"
#include "io_primitives.h"
#include "db_common.h"
#include "utils.h"
#include "bits.h"

constexpr std::uint32_t IMMORTAL = 0x100U;

using namespace std::literals::string_literals;

bool quoted_strings = false;

// Read a quoted or unquoted string depending on DBF_NEW_STRINGS
std::string
read_old_str(istream &in)
{
  if (quoted_strings)
    return db_read_str(in);
  else
    return db_unquoted_str(in);
}

// Reads DBF_NEW_LOCKS locks
lockmap
read_old_locks(istream &in, dbref obj, std::uint32_t)
{
  lockmap locks;

  while (in.peek() == '_') {
    lock l{};
    std::string name;

    std::getline(in, name, '|');
    if (name[0] == '_') {
      name.erase(0, 1);
    } else {
      throw db_format_exception{"Unable to read lock from #"s +
                                std::to_string(obj)};
    }

    l.type = name;
    l.creator = obj;
    l.flags = default_lock_flags(name);
    l.key = read_boolexp(in);

    locks.emplace(name, std::move(l));
  }
  return locks;
}

// Pre DBF_NEW_LOCKS - three fixed locks.
lockmap
read_really_old_locks(istream &in, dbref obj, std::uint32_t)
{
  lockmap locks;
  const char *names[] = {"Basic", "Use", "Enter"};

  for (int i = 0; i < 3; i += 1) {
    char c = in.get();
    if (c == '\n') {
      continue;
    }
    in.unget();
    lock l{};
    l.type = names[i];
    l.creator = obj;
    l.flags = default_lock_flags(names[i]);
    l.key = read_boolexp(in);
    locks.emplace(names[i], std::move(l));
  }

  return locks;
}

attrmap
read_old_attrs(istream &in, uint32_t flags)
{
  attrmap attribs;
  char c;
  std::string line;

  while (in.get(c)) {
    attrib a;
    std::uint32_t aflags;

    switch (c) {
    case ']': {
      std::getline(in, line);
      auto elems = split_on(line, '^');
      if (!(elems.size() == 3 || elems.size() == 4)) {
        throw db_format_exception{"Invalid attribute header "s + line};
      }
      a.name = elems[0];
      a.creator = std::stoi(elems[1]);
      aflags = std::stoi(elems[2]);
      if (!(flags & DBF_AF_VISUAL)) {
        constexpr std::uint32_t AF_ODARK = 0x1U;
        constexpr std::uint32_t AF_VISUAL = 0x400U;
        if (aflags & AF_ODARK) {
          aflags |= AF_VISUAL;
          aflags &= ~AF_ODARK;
        }
      }
      a.flags = attrflags_to_vec(flags);
      if (elems.size() == 4) {
        a.derefs = std::stoi(elems[3]);
      }
      a.data = read_old_str(in);
      attribs.emplace(elems[0], std::move(a));
    } break;
    case '>':
      throw db_format_exception{"Old style attribute format"};
    case '<':
      if (!in.get(c) || c != '\n') {
        throw db_format_exception{"No newline after < in attribute list"};
      }
      return attribs;
    default:
      throw db_format_exception{"Unexpected character read: "s + c};
    }
  }
  throw db_format_exception{"Unexpected end of file"};
}

dbthing
read_old_object(istream &in, dbref d, std::uint32_t flags)
{
  dbthing obj;
  obj.num = d;
  obj.name = read_old_str(in);
  obj.location = db_getref(in);
  obj.contents = db_getref(in);
  obj.exits = db_getref(in);
  obj.next = db_getref(in);
  obj.parent = db_getref(in);
  if (flags & DBF_SPIFFY_LOCKS) {
    obj.locks = read_locks(in, flags);
  } else if (flags & DBF_NEW_LOCKS) {
    // There is a certain irony in my choice of function names
    obj.locks = read_old_locks(in, d, flags);
  } else {
    obj.locks = read_really_old_locks(in, d, flags);
  }
  obj.owner = db_getref(in);
  obj.zone = db_getref(in);
  obj.pennies = db_getref(in);
  if (flags & DBF_NEW_FLAGS) {
    obj.type = dbtype_from_num(db_getref(in));
    obj.flags = split_words(db_read_str(in));
  } else {
    std::uint32_t oldflags = db_getref(in);
    std::uint32_t oldtoggles = db_getref(in);
    obj.type = dbtype_from_oldflags(oldflags);
    obj.flags = flagbits_to_set(obj.type, oldflags, oldtoggles);
  }
  if (flags & DBF_NO_POWERS) {
    // Empty powers
  } else if (flags & DBF_NEW_POWERS) {
    obj.powers = split_words(db_read_str(in));
  } else {
    auto powers = db_getref(in);
    obj.powers = powerbits_to_set(powers);
    if (!(flags & DBF_SPLIT_IMMORTAL)) {
      if (powers & IMMORTAL) {
        obj.powers.insert("No_Pay");
        obj.powers.insert("No_Quota");
      }
    }
  }
  if (!(flags & DBF_NO_CHAT_SYSTEM)) {
    // Discard really old chat field
    db_getref(in);
  }
  if (flags & DBF_WARNINGS) {
    obj.warnings = warnbits_to_vec(db_getref(in));
  }
  if (flags & DBF_CREATION_TIMES) {
    obj.created = db_getref(in);
    obj.modified = db_getref(in);
  } else {
    time_t now;
    std::time(&now);
    obj.created = now;
    obj.modified = now;
  }
  obj.attribs = read_old_attrs(in, flags);

  if (!(flags & DBF_TYPE_GARBAGE)) {
    if (obj.type == dbtype::THING && obj.flags.count("GOING")) {
      obj.type = dbtype::GARBAGE;
    }
  }

  return obj;
}

database
read_db_oldstyle(istream &in, std::uint32_t flags)
{
  database db{};
  char c;
  std::string line;

  quoted_strings = flags & DBF_NEW_STRINGS;

  db.saved_time = get_time();
  db.flags = standard_flags();
  db.powers = standard_powers();
  db.attribs = standard_attribs();

  while (in.get(c)) {
    switch (c) {
    case '~': {
      long len = db_getref(in);
      db.objects.reserve(len);
    } break;
    case '+':
      std::getline(in, line);
      if (line == "FLAGS LIST") { // DBF_NEW_FLAGS ?
        db.flags = read_flags(in);
      } else if (line == "POWER LIST") { // DBF_NEW_POWERS ?
        db.powers = read_flags(in);
      } else {
        throw db_format_exception{"Unrecognized database format!"};
      }
      break;
    case '#': // [[fallthrough]]
    case '&':
      throw db_format_exception{"Old style database."};
    case '!': {
      dbref d = db_getref(in);
      while (static_cast<std::size_t>(d) != db.objects.size()) {
        if (!(flags & DBF_LESS_GARBAGE)) {
          std::cerr << "Missing object #" << db.objects.size()
                    << istream_line(in) << '\n';
        }
        dbthing garbage;
        garbage.num = static_cast<dbref>(db.objects.size());
        db.objects.push_back(std::move(garbage));
      }
      db.objects.push_back(read_old_object(in, d, flags));
    } break;
    case '*':
      std::getline(in, line);
      if (line != "**END OF DUMP***") {
        throw db_format_exception{"Invalid end string "s + line};
      }
      break;
    default:
      throw db_format_exception{"Unexpected character "s + c};
    }
  }

  return db;
}
