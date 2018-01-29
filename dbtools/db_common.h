#pragma once

flagmap read_flags(istream &);
lockmap read_locks(istream &, std::uint32_t);

std::string read_boolexp(istream &);
std::string read_old_str(istream &);
