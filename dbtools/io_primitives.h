// io_primitives
//
// Low-level input and output functions

#pragma once

#include <cstdint>
#include <tuple>

#if __cplusplus >= 201703L
#include <string_view>
using string_view = std::string_view;
#elif defined(HAVE_BOOST_STRING_VIEW)
#include <boost/utility/string_view.hpp>
using string_view = boost::string_view;
#else
#include <boost/utility/string_ref.hpp>
using string_view = boost::string_ref;
#endif

long db_getref(istream &);
std::uint64_t db_getref_u64(istream &);

std::tuple<std::string, int> db_read_labeled_int(istream &);
int db_read_this_labeled_int(istream &, string_view);

std::uint32_t db_getref_u32(istream &);
std::tuple<std::string, std::uint32_t> db_read_labeled_u32(istream &);
std::uint32_t db_read_this_labeled_u32(istream &, string_view);

std::string db_read_str(istream &);
std::string db_unquoted_str(istream &);
std::tuple<std::string, std::string> db_read_labeled_string(istream &);
std::string db_read_this_labeled_string(istream &, string_view);

std::tuple<std::string, dbref> db_read_labeled_dbref(istream &);
dbref db_read_this_labeled_dbref(istream &, string_view);

void db_write_labeled_string(std::ostream &, string_view, string_view);
