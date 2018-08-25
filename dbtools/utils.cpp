#include <sstream>
#include <string>
#include <ctime>
#include <algorithm>
#include <boost/algorithm/string/split.hpp>

#include "database.h"
#include "io_primitives.h"
#include "utils.h"

stringvec
split_on(string_view words, char sep)
{
  using namespace boost::algorithm;
  stringvec res;
  split(res, words, [sep](char c) { return c == sep; }, token_compress_on);
  return res;
}

// Turn a space-seperated list of words into a set of words
stringset
split_words(string_view words)
{
  using namespace boost::algorithm;
  stringset res;
  split(res, words, [](char c) { return c == ' '; }, token_compress_on);
  return res;
}

stringvec
split_words_vec(string_view words)
{
  auto res = split_on(words, ' ');
  std::sort(res.begin(), res.end());
  return res;
}

std::string
join_words(const stringset &words)
{
  std::ostringstream out;
  bool first = true;

  for (const auto &w : words) {
    if (first) {
      first = false;
    } else {
      out << ' ';
    }
    for (char c : w) {
      if (c == '"' || c == '\\') {
        out << '\\';
      }
      out << c;
    }
  }
  return out.str();
}

std::string
join_words(const stringvec &words)
{
  std::ostringstream out;
  bool first = true;

  for (const auto &w : words) {
    if (first) {
      first = false;
    } else {
      out << ' ';
    }
    for (char c : w) {
      if (c == '"' || c == '\\') {
        out << '\\';
      }
      out << c;
    }
  }
  return out.str();
}

std::string
get_time()
{
  std::time_t now = time(nullptr);
  std::string nowstr = std::ctime(&now);
  if (nowstr.back() == '\n') {
    nowstr.pop_back();
  }
  return nowstr;
}
