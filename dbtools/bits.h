// bits.h
//
// Functions for converting numeric bitstrings of flags, powers,
// attribute flags, etc. to sets of strings.

#pragma once

std::map<std::string, attrib> standard_attribs();
stringset attrflags_to_set(std::uint32_t);

std::map<std::string, flag> standard_flags();
stringset flagbits_to_set(dbtype, std::uint32_t, std::uint32_t);

std::map<std::string, flag> standard_powers();
stringset powerbits_to_set(std::uint32_t);

stringset warnbits_to_set(std::uint32_t);

stringset lockbits_to_set(std::uint32_t);
stringset default_lock_flags(const std::string &);

dbtype dbtype_from_oldflags(std::uint32_t);
