#include <string>
#include <cstdint>
#include <algorithm>

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
#include "io_primitives.h"
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

stringvec
privs_to_vec(const PRIV *privs, std::uint32_t bits)
{
  stringvec flags;

  for (int i = 0; privs[i].name; i += 1) {
    if (bits & privs[i].bits_to_show) {
      flags.push_back(privs[i].name);
    }
  }
  std::sort(flags.begin(), flags.end());
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

stringvec
attrflags_to_vec(std::uint32_t bits)
{
  auto flags = privs_to_vec(attr_privs_db, bits);
  return flags;
}

attrmap
standard_attribs()
{
  attrmap attribs;

  for (int i = 0; attr[i].name; i += 1) {
    attrib a{};

    a.name = attr[i].name;
    a.creator = attr[i].creator;
    a.flags = attrflags_to_vec(attr[i].flags);
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

flagmap
build_standard_flags(const FLAG *flag_tab, const FLAG_ALIAS *alias_tab)
{
  flagmap flags;

  for (int i = 0; flag_tab[i].name; i += 1) {
    flag f;
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

flagmap
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

flagmap
standard_powers()
{
  return build_standard_flags(power_table, power_alias_tab);
}

stringset
powerbits_to_set(std::uint32_t bits)
{
  stringset powers;

  for (int i = 0; power_table[i].name; i += 1) {
    if (power_table[i].bitpos & bits) {
      powers.insert(power_table[i].name);
    }
  }

  return powers;
}

stringvec
warnbits_to_vec(std::uint32_t bits)
{
  stringvec warnings;

  for (int i = 0; checklist[i].name; i += 1) {
    if (checklist[i].flag & bits) {
      warnings.push_back(checklist[i].name);
    }
  }
  std::sort(warnings.begin(), warnings.end());
  return warnings;
}

stringvec
lockbits_to_vec(std::uint32_t bits)
{
  return privs_to_vec(lock_privs, bits);
}

stringvec
default_lock_flags(string_view name)
{
  for (int i = 0; lock_types[i].type; i += 1) {
    if (name == lock_types[i].type) {
      return privs_to_vec(lock_privs, lock_types[i].flags);
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
    throw db_format_exception{"Unknown type "s +
                              std::to_string(bits & OLD_TYPE_MASK)};
  }
}
