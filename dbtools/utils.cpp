#include <sstream>
#include <string>
#include <set>
#include <ctime>

#include <boost/algorithm/string/split.hpp>

#include "database.h"
#include "utils.h"

std::vector<std::string>
split_on(const std::string &words, char sep)
{
  std::vector<std::string> res;
  boost::algorithm::split(res, words, [sep](char c){ return c == sep; },
                          boost::algorithm::token_compress_on);
  return res;
}

// Turn a space-seperated list of words into a set of words
stringset
split_words(const std::string &words)
{
  using namespace boost::algorithm;
  stringset res;
  split(res, words, [](char c){ return c == ' '; }, token_compress_on);
  return res;
}

std::string
join_words(const stringset &words)
{
  std::ostringstream out;

  for (const auto &w : words) {
    out << w;
    out << ' ';
  }

  auto res = out.str();

  if (res.back() == ' ') {
    res.pop_back();
  }

  return res;
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
