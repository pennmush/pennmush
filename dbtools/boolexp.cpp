#include <iostream>
#include <string>
#include <sstream>
#include <memory>
#include <cctype>

#if __cplusplus >= 201703L
#include <optional>
using std::optional;
#else
#include <boost/optional.hpp>
using boost::optional;
#endif

#include "database.h"
#include "db_common.h"
#include "io_primitives.h"

using namespace std::literals::string_literals;

class boolexp {
public:
  virtual ~boolexp() {}
  virtual std::ostream& print(std::ostream &, bool) = 0;
};

class not_boolexp : public boolexp {
  std::unique_ptr<boolexp> be;
public:
  not_boolexp(std::unique_ptr<boolexp> &&b)
    : be(std::move(b)) {}
  std::ostream & print(std::ostream &out, bool) override {
    out << '!';
    be->print(out, true);
    return out;
  }
};

class simple_boolexp : public boolexp {
  char flag;
  dbref obj;
public:
  simple_boolexp(char f, dbref o) : flag(f), obj(o) {}
  std::ostream & print(std::ostream &out, bool) override {
    if (flag) {
      out << flag;
    }
    out << '#' << obj;
    return out;
  }
};

class ind_boolexp : public boolexp {
  dbref obj;
  optional<std::string> lock;
public:
  ind_boolexp(dbref o) : obj(o) {}
  ind_boolexp(dbref o, string_view l) : obj(o), lock(l) {}
  std::ostream& print(std::ostream &out, bool) override {
    out << "@#" << obj;
    if (lock) {
      out << '/' << *lock;
    }
    return out;
  }
};

class is_boolexp : public simple_boolexp {
public:
  is_boolexp(dbref o) : simple_boolexp('=', o) {}
};

class carry_boolexp : public simple_boolexp {
public:
  carry_boolexp(dbref o) : simple_boolexp('+', o) {}
};

class owner_boolexp : public simple_boolexp {
public:
  owner_boolexp(dbref o) : simple_boolexp('$', o) {}
};

class const_boolexp : public simple_boolexp {
public:
  const_boolexp(dbref o) : simple_boolexp('\0', o) {}
};

class bool_boolexp : public boolexp {
  bool val;
public:
  bool_boolexp(bool v) : val(v) {}
  std::ostream & print(std::ostream &out, bool) override {
    if (val) {
      out << "#true";
    } else {
      out << "#false";
    }
    return out;
  }
};

class pair_boolexp : public boolexp {
  std::string name;
  char flag;
  std::string val;
public:
  pair_boolexp(string_view n, char f, string_view v)
    : name(n.data()), flag(f), val(v.data()) {}
  std::ostream & print(std::ostream &out, bool) override {
    out << name << flag << val;
    return out;
  }
};

class atr_boolexp : public pair_boolexp {
public:
  atr_boolexp(string_view n, string_view v)
    : pair_boolexp(n, ':', v) {}
};

class eval_boolexp : public pair_boolexp {
public:
  eval_boolexp(string_view n, string_view v)
    : pair_boolexp(n, '/', v) {}
};

class flag_boolexp : public pair_boolexp {
public:
  flag_boolexp(string_view n, string_view v)
    : pair_boolexp(n, '^', v) {}
};

class andor_boolexp : public boolexp {
  std::unique_ptr<boolexp> left;
  char flag;
  std::unique_ptr<boolexp> right;
public:
  andor_boolexp(std::unique_ptr<boolexp> &&l,
              char f,
              std::unique_ptr<boolexp> &&r)
    : left(std::move(l)), flag(f), right(std::move(r)) {}
  std::ostream & print(std::ostream &out, bool pnot) override {
    bool pand = flag == '&';
    if (pnot) {
      out << '(';
    }
    left->print(out, pand);
    out << flag;
    right->print(out, pand);
    if (pnot) {
      out << ')';
    }
    return out;
  }
};

class and_boolexp : public andor_boolexp {
public:
  and_boolexp(std::unique_ptr<boolexp> &&l, std::unique_ptr<boolexp> &&r)
    : andor_boolexp(std::move(l), '&', std::move(r)) {}
};

class or_boolexp : public andor_boolexp {
public:
  or_boolexp(std::unique_ptr<boolexp> &&l, std::unique_ptr<boolexp> &&r)
    : andor_boolexp(std::move(l), '|', std::move(r)) {}
};

void
close_paren(istream &in)
{
  char c;
  if (!(in.get(c) && c == ')')) {
    throw db_format_exception{"Expected to read ), got "s + c + istream_line(in)};
  }
}

std::unique_ptr<boolexp>
parse_boolexp(istream &in)
{
  char c;
  if (!in.get(c)) {
    throw db_format_exception{"Unable to read full lock at"s + istream_line(in)};
  }
  
  if (c == '(') {
    in.get(c);
    dbref d;
    switch (c) {
    case '=':
      in >> d;
      close_paren(in);
      return std::make_unique<is_boolexp>(d);
    case '+':
      in >> d;
      close_paren(in);
      return std::make_unique<carry_boolexp>(d);
    case '$':
      in >> d;
      close_paren(in);
      return std::make_unique<owner_boolexp>(d);
    case '@':
      in >> d;
      close_paren(in);
      return std::make_unique<ind_boolexp>(d);
    case '!': {
      auto b = parse_boolexp(in);
      close_paren(in);
      return std::make_unique<not_boolexp>(std::move(b));
    }
    default: {
      auto l = parse_boolexp(in);
      char c2;
      in.get(c2);
      switch (c2) {
      case '&': {
        auto r = parse_boolexp(in);
        close_paren(in);
        return std::make_unique<and_boolexp>(std::move(l), std::move(r));
      }
      case '|': {
        auto r = parse_boolexp(in);
        close_paren(in);
        return std::make_unique<or_boolexp>(std::move(l), std::move(r));
      }
      default:
        throw db_format_exception{"Invalid character in lock: "s + c2 + istream_line(in)};
      }
    }
    }
  } else if (std::isdigit(c)) {
    in.unget();
    dbref d;
    in >> d;
    return std::make_unique<const_boolexp>(d);
  } else if (c == '"') {
    std::string key = db_read_str(in);
    char c2 = in.get();
    std::string val = db_read_str(in);
    switch (c2) {
    case ':':
      return std::make_unique<atr_boolexp>(key, val);
    case '/':
      return std::make_unique<eval_boolexp>(key, val);
    case '^':
      return std::make_unique<flag_boolexp>(key, val);
    default:
      throw db_format_exception{"Invalid character in lock: "s + c2 + istream_line(in)};
    }
  } else {
          throw db_format_exception{"Invalid character in lock: "s + c + istream_line(in)};
  }
}

// Read a DBF_NEW_LOCKS boolexp
std::string
read_boolexp(istream &in)
{
  auto be = parse_boolexp(in);

  while (in.peek() == '\n') {
    in.get();
  }
  
  std::ostringstream out;
  be->print(out, false);
  std::cout << "Read lock: " << out.str() << '\n';
  return out.str();
}
