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

#include "oldattrb.h"

using namespace std::literals::string_literals;

class boolexp
{
public:
  virtual ~boolexp() {}
  virtual std::ostream &print(std::ostream &, bool) = 0;
};

class not_boolexp : public boolexp
{
  std::unique_ptr<boolexp> be;

public:
  not_boolexp(std::unique_ptr<boolexp> &&b) : be(std::move(b)) {}
  std::ostream &
  print(std::ostream &out, bool) override
  {
    out << '!';
    return be->print(out, true);
  }
};

class simple_boolexp : public boolexp
{
  char flag;
  dbref obj;

public:
  simple_boolexp(char f, dbref o) : flag(f), obj(o) {}
  std::ostream &
  print(std::ostream &out, bool) override
  {
    if (flag) {
      out << flag;
    }
    return out << '#' << obj;
  }
};

class ind_boolexp : public boolexp
{
  dbref obj;
  optional<std::string> lock;

public:
  ind_boolexp(dbref o) : obj(o) {}
  ind_boolexp(dbref o, std::string l) : obj(o), lock(std::move(l)) {}
  std::ostream &
  print(std::ostream &out, bool) override
  {
    out << "@#" << obj;
    if (lock) {
      out << '/' << *lock;
    }
    return out;
  }
};

class is_boolexp : public simple_boolexp
{
public:
  is_boolexp(dbref o) : simple_boolexp('=', o) {}
};

class carry_boolexp : public simple_boolexp
{
public:
  carry_boolexp(dbref o) : simple_boolexp('+', o) {}
};

class owner_boolexp : public simple_boolexp
{
public:
  owner_boolexp(dbref o) : simple_boolexp('$', o) {}
};

class const_boolexp : public simple_boolexp
{
public:
  const_boolexp(dbref o) : simple_boolexp('\0', o) {}
};

class bool_boolexp : public boolexp
{
  bool val;

public:
  bool_boolexp(bool v) : val(v) {}
  std::ostream &
  print(std::ostream &out, bool) override
  {
    if (val) {
      out << "#true";
    } else {
      out << "#false";
    }
    return out;
  }
};

class pair_boolexp : public boolexp
{
  std::string name;
  char flag;
  std::string val;

public:
  pair_boolexp(std::string n, char f, std::string v)
      : name(std::move(n)), flag(f), val(std::move(v))
  {
  }
  std::ostream &
  print(std::ostream &out, bool) override
  {
    return out << name << flag << val;
  }
};

class atr_boolexp : public pair_boolexp
{
public:
  atr_boolexp(const std::string &n, const std::string &v)
      : pair_boolexp(n, ':', v)
  {
  }
};

class eval_boolexp : public pair_boolexp
{
public:
  eval_boolexp(const std::string &n, const std::string &v)
      : pair_boolexp(n, '/', v)
  {
  }
};

class flag_boolexp : public pair_boolexp
{
public:
  flag_boolexp(const std::string &n, const std::string &v)
      : pair_boolexp(n, '^', v)
  {
  }
};

class andor_boolexp : public boolexp
{
  std::unique_ptr<boolexp> left;
  char flag;
  std::unique_ptr<boolexp> right;

public:
  andor_boolexp(std::unique_ptr<boolexp> &&l, char f,
                std::unique_ptr<boolexp> &&r)
      : left(std::move(l)), flag(f), right(std::move(r))
  {
  }
  std::ostream &
  print(std::ostream &out, bool pnot) override
  {
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

class and_boolexp : public andor_boolexp
{
public:
  and_boolexp(std::unique_ptr<boolexp> &&l, std::unique_ptr<boolexp> &&r)
      : andor_boolexp(std::move(l), '&', std::move(r))
  {
  }
};

class or_boolexp : public andor_boolexp
{
public:
  or_boolexp(std::unique_ptr<boolexp> &&l, std::unique_ptr<boolexp> &&r)
      : andor_boolexp(std::move(l), '|', std::move(r))
  {
  }
};

void
close_paren(istream &in)
{
  char c;
  if (!(in.get(c) && c == ')')) {
    throw db_format_exception{"Expected to read ), got "s + c +
                              istream_line(in)};
  }
}

// Copied from 1.7.3 source
const char *
convert_atr(int oldatr)
{
  static char result[3];

  switch (oldatr) {
  case A_OSUCC:
    return "OSUCCESS";
  case A_OFAIL:
    return "OFAILURE";
  case A_FAIL:
    return "FAILURE";
  case A_SUCC:
    return "SUCCESS";
  case A_PASS:
    return "XYXXY";
  case A_DESC:
    return "DESCRIBE";
  case A_SEX:
    return "SEX";
  case A_ODROP:
    return "ODROP";
  case A_DROP:
    return "DROP";
  case A_OKILL:
    return "OKILL";
  case A_KILL:
    return "KILL";
  case A_ASUCC:
    return "ASUCCESS";
  case A_AFAIL:
    return "AFAILURE";
  case A_ADROP:
    return "ADROP";
  case A_AKILL:
    return "AKILL";
  case A_USE:
    return "DOES";
  case A_CHARGES:
    return "CHARGES";
  case A_RUNOUT:
    return "RUNOUT";
  case A_STARTUP:
    return "STARTUP";
  case A_ACLONE:
    return "ACLONE";
  case A_APAY:
    return "APAYMENT";
  case A_OPAY:
    return "OPAYMENT";
  case A_PAY:
    return "PAYMENT";
  case A_COST:
    return "COST";
  case A_RAND:
    return "RAND";
  case A_LISTEN:
    return "LISTEN";
  case A_AAHEAR:
    return "AAHEAR";
  case A_AMHEAR:
    return "AMHEAR";
  case A_AHEAR:
    return "AHEAR";
  case A_LAST:
    return "LAST";
  case A_QUEUE:
    return "QUEUE";
  case A_IDESC:
    return "IDESCRIBE";
  case A_ENTER:
    return "ENTER";
  case A_OXENTER:
    return "OXENTER";
  case A_AENTER:
    return "AENTER";
  case A_ADESC:
    return "ADESCRIBE";
  case A_ODESC:
    return "ODESCRIBE";
  case A_RQUOTA:
    return "RQUOTA";
  case A_ACONNECT:
    return "ACONNECT";
  case A_ADISCONNECT:
    return "ADISCONNECT";
  case A_LEAVE:
    return "LEAVE";
  case A_ALEAVE:
    return "ALEAVE";
  case A_OLEAVE:
    return "OLEAVE";
  case A_OENTER:
    return "OENTER";
  case A_OXLEAVE:
    return "OXLEAVE";
  default:
    if (oldatr >= 100 && oldatr < 178) {
      result[0] = 'V' + (oldatr - 100) / 26;
      result[1] = 'A' + (oldatr - 100) % 26;
      return result;
    } else {
      throw db_format_exception{"Invalid attribute number in convert_atr"};
    }
  }
  /*NOTREACHED */
  return "";
}

std::unique_ptr<boolexp>
parse_boolexp(istream &in)
{
  char c;
  if (!in.get(c)) {
    throw db_format_exception{"Unable to read full lock at"s +
                              istream_line(in)};
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
      in.unget();
      auto l = parse_boolexp(in);
      char c2;
      in.get(c2);
      if (c2 == '&') {
        auto r = parse_boolexp(in);
        close_paren(in);
        return std::make_unique<and_boolexp>(std::move(l), std::move(r));
      } else if (c2 == '|') {
        auto r = parse_boolexp(in);
        close_paren(in);
        return std::make_unique<or_boolexp>(std::move(l), std::move(r));
      } else {
        throw db_format_exception{"Invalid character in lock: "s + c2 +
                                  istream_line(in)};
      }
    }
    }
  } else if (std::isdigit(c)) {
    in.unget();
    dbref d;
    in >> d;
    if (in.peek() == ':') {
      in.get();
      auto key = convert_atr(d);
      std::string val;
      char c2;
      while (in.get(c2)) {
        if (c2 == '\n' || c2 == ')') {
          break;
        }
        val.push_back(c2);
      }
      if (in.eof()) {
        throw db_format_exception{"Unexpected EOF reading old-style atr lock"s +
                                  istream_line(in)};
      }
      return std::make_unique<atr_boolexp>(key, val);
    }
    return std::make_unique<const_boolexp>(d);
  } else if (c == '"') {
    in.unget();
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
      throw db_format_exception{"Invalid character in lock: "s + c2 +
                                istream_line(in)};
    }
  } else if (c == '-') {
    // 1.7.6 source calls this an obsolete NOTHING key and eats it.
    while (in.peek() != '\n') {
      in.get();
    }
  } else {
    // Unquoted attr or eval lock?
    in.unget();
    std::string key;
    char c2;
    while (in.get(c2)) {
      if (c2 == ':' || c2 == '/' || c2 == '\n') {
        break;
      }
      key.push_back(c2);
    }
    if (in.eof() || c2 == '\n') {
      throw db_format_exception{"Invalid lock"s + istream_line(in)};
    }
    char type = c2;
    std::string val;
    while (in.get(c2)) {
      if (c2 == '\n' || c2 == ')' || c2 == '&' || c2 == '|') {
        break;
      }
      val.push_back(c2);
    }
    if (in.eof()) {
      throw db_format_exception{"Unexpected end of file in lock."};
    }
    in.unget();
    if (type == ':') {
      return std::make_unique<atr_boolexp>(key, val);
    } else {
      return std::make_unique<eval_boolexp>(key, val);
    }
  }
  // Should never be reached
  throw db_format_exception{"Unable to read lock!"s + istream_line(in)};
}

// Read a DBF_NEW_LOCKS boolexp
std::string
read_boolexp(istream &in)
{
  auto be = parse_boolexp(in);

  if (in.peek() == '\n') {
    in.get();
  } else {
    throw db_format_exception{"Invalid character in lock: "s +
                              static_cast<char>(in.peek()) + istream_line(in)};
  }

  std::ostringstream out;
  be->print(out, false);
  // std::cout << "Read lock: " << out.str() << '\n';
  return out.str();
}
