#include <map>
#include <set>
#include <string>
#include <cstdint>

#ifndef _MSC_VER
#include "config.h"
#else
#define HAVE_STDINT_H
#define HAVE_INTTYPES_H
#define SSE_OFFSET 0
#define WIN32_CDECL __cdecl
#define __attribute__(x)
#define RESTRICT
#endif

#include "hdrs/privtab.h"
#include "hdrs/atr_tab.h"
#include "hdrs/lock.h"
#include "hdrs/lock_tab.h"
#include "hdrs/oldflags.h"
#include "hdrs/flags.h"
#include "hdrs/flag_tab.h"
#include "hdrs/warn_tab.h"

#include "database.h"
#include "bits.h"

using namespace std::literals::string_literals;

stringset
privs_to_set(const PRIV *privs, std::uint32_t bits)
{
  stringset flags;

  for (int i = 0; privs[i].name; i += 1) {
    if (bits & privs[i].bits_to_show) {
      flags.insert(privs[i].name);
    }
  }

  return flags;
}

stringset
flagprivs_to_set(std::uint32_t bits)
{
  return privs_to_set(flag_privs, bits);
}

stringset
typebits_to_set(std::uint32_t bits)
{
  stringset types;
  for (int i = 0; type_table[i].name; i += 1) {
    if (bits & type_table[i].perms) {
      types.insert(type_table[i].name);
    }
  }
  return types;
}

stringset
attrflags_to_set(std::uint32_t bits)
{
  return privs_to_set(attr_privs_db, bits);
}

std::map<std::string, attrib>
standard_attribs()
{
  std::map<std::string, attrib> attribs;

  for (int i = 0; attr[i].name; i += 1) {
    attrib a{};

    a.name = attr[i].name;
    a.creator = attr[i].creator;
    a.flags = attrflags_to_set(attr[i].flags);
    attribs.emplace(attr[i].name, std::move(a));
  }

  for (int i = 0; attralias[i].realname; i += 1) {
    auto a = attribs.find(attralias[i].realname);
    if (a != attribs.end()) {
      attribs.emplace(attralias[i].alias, a->second);
    }
  }

  return attribs;
}

std::map<std::string, flag>
build_standard_flags(const FLAG *flag_tab, const FLAG_ALIAS *alias_tab)
{
  std::map<std::string, flag> flags;

  for (int i = 0; flag_tab[i].name; i += 1) {
    flag f{};
    f.name = flag_tab[i].name;
    f.letter = flag_tab[i].letter;
    f.types = typebits_to_set(flag_tab[i].type);
    f.perms = flagprivs_to_set(flag_tab[i].perms);
    f.negate_perms = flagprivs_to_set(flag_tab[i].negate_perms);
    flags.emplace(flag_tab[i].name, std::move(f));
  }

  for (int i = 0; alias_tab[i].realname; i += 1) {
    auto f = flags.find(alias_tab[i].realname);
    if (f != flags.end()) {
      flags.emplace(alias_tab[i].alias, f->second);
    }
  }
  
  return flags;
}

std::map<std::string, flag>
standard_flags()
{
  return build_standard_flags(flag_table, flag_alias_tab);
}

stringset
flagbits_to_set(dbtype type, std::uint32_t bits, std::uint32_t toggles)
{
  stringset flags;
  std::uint32_t typebit = dbtype_to_num(type);

  for (int i = 0; flag_table[i].name; i += 1) {
    if (flag_table[i].type == NOTYPE && flag_table[i].bitpos & bits) {
      flags.insert(flag_table[i].name);
    } else if (flag_table[i].type & typebit && flag_table[i].bitpos & toggles) {
      flags.insert(flag_table[i].name);
    }
  }

  for (int i = 0; hack_table[i].name; i += 1) {
    if (hack_table[i].type & typebit && hack_table[i].bitpos & toggles) {
      flags.insert(hack_table[i].name);
    }
  }
  
  return flags;
}

std::map<std::string, flag>
standard_powers()
{
  return build_standard_flags(power_table, power_alias_tab);
}

stringset
powerbits_to_set(std::uint32_t bits) {
  stringset powers;

  for (int i = 0; power_table[i].name; i += 1) {
    if (power_table[i].bitpos & bits) {
      powers.insert(power_table[i].name);
    }
  }

  return powers;
}

stringset
warnbits_to_set(std::uint32_t bits)
{
  stringset warnings;

  for (int i = 0; checklist[i].name; i += 1) {
    if (checklist[i].flag & bits) {
      warnings.insert(checklist[i].name);
    }
  }

  return warnings;
}

stringset
lockbits_to_set(std::uint32_t bits) {
  return privs_to_set(lock_privs, bits);
}

stringset
default_lock_flags(const std::string &name)
{
  for (int i = 0; lock_types[i].type; i += 1) {
    if (name == lock_types[i].type) {
      return privs_to_set(lock_privs, lock_types[i].flags); 
    }
  }
  return {};
}

dbtype
dbtype_from_oldflags(std::uint32_t bits)
{
  switch (bits & OLD_TYPE_MASK) {
  case OLD_TYPE_PLAYER:
    return dbtype::PLAYER;
  case OLD_TYPE_ROOM:
    return dbtype::ROOM;
  case OLD_TYPE_THING:
    return dbtype::THING;
  case OLD_TYPE_EXIT:
    return dbtype::EXIT;
  case OLD_TYPE_GARBAGE:
    return dbtype::GARBAGE;
  default:
    throw db_format_exception{"Unknown type "s + std::to_string(bits & OLD_TYPE_MASK)};
  }
}
