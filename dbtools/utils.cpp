#include <sstream>
#include <string>
#include <set>
#include <ctime>

#define USE_BOOST

#ifdef USE_BOOST
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#else
#include <regex>
#endif

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
strset
split_words(const std::string &words)
{
#ifdef USE_BOOST
  using namespace boost::algorithm;
  strset res;
  split(res, words, [](char c){ return c == ' '; } /* is_any_of(" ") */, token_compress_on);
  return res;
#else
  static std::regex whitespace{"\\s+"};
  strset res;

  for (auto i =
         std::sregex_token_iterator(words.begin(), words.end(), whitespace, -1);
       i != std::sregex_token_iterator{}; ++i) {
    res.insert(i->str());
  }

  return res;
#endif
}

std::string
join_words(const strset &words)
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
