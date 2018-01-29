#include <iostream>
#include <string>
#include <limits>
#include <tuple>

#include "database.h"
#include "io_primitives.h"

using namespace std::literals::string_literals;

void
skip_space(istream &in)
{
  while (in.peek() == ' ')
    in.get();
}

void
chomp(istream &in)
{
  in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

template <typename T>
auto
db_read(istream &in) -> T
{
  T val;
  if (!(in >> val)) {
    throw db_format_exception{"Unable to read value from database"s +
                              istream_line(in)};
  }
  chomp(in);
  return val;
}

long
db_getref(istream &in)
{
  return db_read<long>(in);
}

std::uint32_t
db_getref_u32(istream &in)
{
  return db_read<std::uint32_t>(in);
}

std::uint64_t
db_getref_u64(istream &in)
{
  return db_read<std::uint64_t>(in);
}

// Read a quoted string
std::string
db_read_str(istream &in)
{
  char c;
  std::string val;

  in.get(c);

  if (c != '"') {
    throw db_format_exception{"String missing leading \""s + istream_line(in)};
  }

  while (in.get(c)) {
    if (c == '\\') {
      in.get(c);
      val.push_back(c);
    } else if (c == '"') {
      chomp(in);
      return val;
    } else {
      val.push_back(c);
    }
  }
  throw db_format_exception{"String without ending \""s + istream_line(in)};
}

// Read an old-school unquoted string. Embedded newlines are \r\n, end
// of string is \n
std::string
db_unquoted_str(istream &in)
{
  char c;
  std::string val;

  while (in.get(c)) {
    if (c == '\n') {
      if (val.size() && val.back() == '\r') {
        val.back() = '\n';
      } else {
        return val;
      }
    } else {
      val.push_back(c);
    }
  }

  throw db_format_exception{
    "Unexpected end of file while trying to read unquoted string"};
}

std::string
read_label(istream &in)
{
  std::string lbl;

  if (!(in >> lbl)) {
    throw db_format_exception{"Unable to read label"s + istream_line(in)};
  }
  skip_space(in);
  return lbl;
}

std::tuple<std::string, int>
db_read_labeled_int(istream &in)
{
  std::string lbl;
  int n;
  if (!(in >> lbl >> n)) {
    throw db_format_exception{"Unable to read labeled int"s + istream_line(in)};
  }
  chomp(in);
  return std::make_tuple(lbl, n);
}

std::tuple<std::string, std::uint32_t>
db_read_labeled_u32(istream &in)
{
  std::string lbl;
  std::uint32_t n;
  if (!(in >> lbl >> n)) {
    throw db_format_exception{"Unable to read labeled uint32_t"s +
                              istream_line(in)};
  }
  chomp(in);
  return std::make_tuple(lbl, n);
}

void
raise_label_error(const istream &in, string_view expected, string_view got)
{
  std::string err = "Expected label '";
  err.append(expected.data());
  err.append("', but read '");
  err.append(got.data());
  err.push_back('\'');
  err.append(istream_line(in));
  throw db_format_exception{err};
}

int
db_read_this_labeled_int(istream &in, string_view lbl)
{
  std::string got;
  int n;
  std::tie(got, n) = db_read_labeled_int(in);
  if (got != lbl) {
    raise_label_error(in, lbl, got);
  }
  return n;
}

std::uint32_t
db_read_this_labeled_u32(istream &in, string_view lbl)
{
  std::string got;
  std::uint32_t n;
  std::tie(got, n) = db_read_labeled_u32(in);
  if (got != lbl) {
    raise_label_error(in, lbl, got);
  }
  return n;
}

std::tuple<std::string, std::string>
db_read_labeled_string(istream &in)
{
  auto lbl = read_label(in);
  auto body = db_read_str(in);
  return std::make_tuple(lbl, body);
}

std::string
db_read_this_labeled_string(istream &in, string_view lbl)
{
  auto got = read_label(in);
  if (got != lbl) {
    raise_label_error(in, lbl, got);
  }
  return db_read_str(in);
}

std::tuple<std::string, dbref>
db_read_labeled_dbref(istream &in)
{
  auto lbl = read_label(in);
  char c;

  in.get(c);
  if (c != '#') {
    throw db_format_exception{"Malformed dbref label: "s + lbl +
                              istream_line(in)};
  }

  dbref d;
  if (!(in >> d)) {
    throw db_format_exception{"Malformed dbref 2" + istream_line(in)};
  }

  chomp(in);

  return std::make_tuple(lbl, d);
}

dbref
db_read_this_labeled_dbref(istream &in, string_view lbl)
{
  std::string got;
  dbref d;

  std::tie(got, d) = db_read_labeled_dbref(in);

  if (got != lbl) {
    raise_label_error(in, lbl, got);
  }

  return d;
}

void
db_write_labeled_string(std::ostream &out, string_view lbl, string_view val)
{
  out << lbl << " \"";
  for (char c : val) {
    if (c == '"' || c == '\\') {
      out << '\\';
    }
    out << c;
  }
  out << "\"\n";
}
