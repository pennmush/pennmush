#pragma once

std::map<std::string, flag> read_flags(istream &);
std::map<std::string, lock> read_locks(istream &, std::uint32_t);

std::string read_boolexp(istream &);
