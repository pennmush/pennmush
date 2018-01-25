// bits.h
//
// Functions for converting numeric bitstrings of flags, powers,
// attribute flags, etc. to sets of strings.

#pragma once

std::map<std::string, attrib> standard_attribs();
strset attrflags_to_set(std::uint32_t);

std::map<std::string, flag> standard_flags();
strset flagbits_to_set(dbtype, std::uint32_t, std::uint32_t);

std::map<std::string, flag> standard_powers();
strset powerbits_to_set(std::uint32_t);

strset warnbits_to_set(std::uint32_t);

strset lockbits_to_set(std::uint32_t);
strset default_lock_flags(const std::string &);

dbtype dbtype_from_oldflags(std::uint32_t);
