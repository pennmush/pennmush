#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <random>
#include <ctime>

#ifdef WIN32
#include <Windows.h>
#include <Bcrypt.h>
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS 0
#endif
#else
#include <openssl/sha.h>
#endif

#include "hasher.h"

std::string
password_hasher::salt(void)
{
  static const char salts[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

  static std::random_device rd;
  static std::ranlux48 rng{rd()};
  std::uniform_int_distribution<int> pick(0, sizeof salts - 1);

  char salt[2];
  salt[0] = salts[pick(rng)];
  salt[1] = salts[pick(rng)];

  return std::string(salt, 2);
}

std::string
password_hasher::make_password(const std::string &plain)
{
  std::ostringstream sink;

  auto saltchars = salt();

  auto hashed = hash(saltchars, plain);

  sink << "2:" << algo() << ':' << saltchars << std::hex << std::setfill('0');
  for (unsigned char b : hashed) {
    sink << std::setw(2) << static_cast<unsigned>(b);
  }
  sink << std::dec << ':' << std::time(nullptr);

  return sink.str();
}

#ifdef WIN32

class win32_sha256_hasher : public password_hasher
{
public:
  win32_sha256_hasher();
  ~win32_sha256_hasher();

protected:
  const char *
  algo() const override
  {
    return "sha256";
  }
  std::string hash(const std::string &, const std::string &) override;

private:
  BCRYPT_ALG_HANDLE balgo;
  DWORD hashlen = 0;
};

win32_sha256_hasher::win32_sha256_hasher()
{
  if (BCryptOpenAlgorithmProvider(&balgo, BCRYPT_SHA256_ALGORITHM, NULL, 0) !=
      STATUS_SUCCESS) {
    throw std::runtime_error("Unable to open bcrypt algorithm provider");
  }
  ULONG cbhash = 0;
  if (BCryptGetProperty(balgo, BCRYPT_HASH_LENGTH, (PBYTE) &hashlen,
                        sizeof(hashlen), &cbhash, 0) != STATUS_SUCCESS) {
    BCryptCloseAlgorithmProvider(balgo, 0);
    throw std::runtime_error("Unable to fetch hash length");
  }
}

win32_sha256_hasher::~win32_sha256_hasher()
{
  BCryptCloseAlgorithmProvider(balgo, 0);
}

std::string
win32_sha256_hasher::hash(const std::string &saltchars,
                          const std::string &plain)
{
  BCRYPT_HASH_HANDLE hfun;
  if (BCryptCreateHash(balgo, &hfun, NULL, 0, NULL, 0, 0) != STATUS_SUCCESS) {
    throw std::runtime_error("Unable to create bcrypt hash algorithm");
  }
  BCryptHashData(hfun, (PUCHAR) saltchars.data(),
                 static_cast<ULONG>(saltchars.size()), 0);
  BCryptHashData(hfun, (PUCHAR) plain.data(), static_cast<ULONG>(plain.size()),
                 0);
  std::string hashed(hashlen, '\0');
  BCryptFinishHash(hfun, (PUCHAR) hashed.data(), hashlen, 0);
  BCryptDestroyHash(hfun);
  return hashed;
}

#else

class openssl_sha256_hasher : public password_hasher
{
protected:
  const char *
  algo() const override
  {
    return "sha256";
  }
  std::string hash(const std::string &, const std::string &) override;
};

std::string
openssl_sha256_hasher::hash(const std::string &saltchars,
                            const std::string &plain)
{
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, saltchars.data(), saltchars.size());
  SHA256_Update(&ctx, plain.data(), plain.size());
  std::string hashed(SHA256_DIGEST_LENGTH, '\0');
  SHA256_Final(reinterpret_cast<unsigned char *>(&hashed[0]), &ctx);
  return hashed;
}

#endif

std::unique_ptr<password_hasher>
make_password_hasher()
{
#ifdef WIN32
  return std::make_unique<win32_sha256_hasher>();
#else
  return std::make_unique<openssl_sha256_hasher>();
#endif
}
