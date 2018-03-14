// bits.h
//
// Functions for converting numeric bitstrings of flags, powers,
// attribute flags, etc. to sets of strings.

#pragma once

attrmap standard_attribs();
stringvec attrflags_to_vec(std::uint32_t);

flagmap standard_flags();
stringset flagbits_to_set(dbtype, std::uint32_t, std::uint32_t);

flagmap standard_powers();
stringset powerbits_to_set(std::uint32_t);

stringvec warnbits_to_vec(std::uint32_t);

stringvec lockbits_to_vec(std::uint32_t);
stringvec default_lock_flags(string_view);

dbtype dbtype_from_oldflags(std::uint32_t);
