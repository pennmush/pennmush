#pragma once

class password_hasher
{
public:
  virtual ~password_hasher(){};
  std::string make_password(const std::string &);

protected:
  std::string salt(void);
  virtual const char *algo() const = 0;
  virtual std::string hash(const std::string &, const std::string &) = 0;
};

std::unique_ptr<password_hasher> make_password_hasher();
