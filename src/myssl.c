/**
 * \file myssl.c
 *
 * \brief Code to support SSL connections in PennMUSH.
 *
 * This file contains nearly all of the code that interacts with the
 * OpenSSL libraries to suppose SSL connections in PennMUSH.
 *
 * Lots of stuff here taken from Eric Rescorla's 2001 Linux Journal
 * articles "An Introduction to OpenSSL Programming"
 */

#include "copyrite.h"

#include <stdio.h>
#include <stdarg.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#include <io.h>
#include <ntstatus.h>
void shutdown_checkpoint(void);
#else /* !WIN32 */
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif
#include <sys/ioctl.h>
#include <errno.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#endif /* !WIN32 */
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/dh.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include "myssl.h"
#include "pcg_basic.h"
#include "conf.h"
#include "parse.h"
#include "wait.h"
#include "confmagic.h"

#define MYSSL_RB 0x1         /**< Read blocked (on read) */
#define MYSSL_WB 0x2         /**< Write blocked (on write) */
#define MYSSL_RBOW 0x4       /**< Read blocked (on write) */
#define MYSSL_WBOR 0x8       /**< Write blocked (on read) */
#define MYSSL_ACCEPT 0x10    /**< We need to call SSL_accept (again) */
#define MYSSL_VERIFIED 0x20  /**< This is an authenticated connection */
#define MYSSL_HANDSHAKE 0x40 /**< We need to call SSL_do_handshake */

#undef MYSSL_DEBUG
#ifdef MYSSL_DEBUG
#define ssl_debugdump(x) ssl_errordump(x)
#else
#define ssl_debugdump(x)
#endif

static void ssl_errordump(const char *msg);
static int client_verify_callback(int preverify_ok, X509_STORE_CTX *x509_ctx);
static DH *get_dh2048(void);

static BIO *bio_err = NULL;
static SSL_CTX *ctx = NULL;

static const char *
time_string(void)
{
  static char buffer[100];
  time_t now;
  struct tm *ltm;

  now = time(NULL);
  ltm = localtime(&now);
  strftime(buffer, 100, "[%Y-%m-%d %H:%M:%S]", ltm);

  return buffer;
}

/** Generate 128 bits of random noise for seeding RNGs. Attempts to
 * use various OS-specific sources of random bits, with a fallback
 * based on time and pid. */
void
generate_seed(uint64_t seeds[])
{
  bool seed_generated = false;
  static int stream_count = 0;
  int len = sizeof(uint64_t) * 2;

#ifdef HAVE_GETENTROPY
  /* On OpenBSD and up to date Linux, use getentropy() to avoid the
     open/read/close sequence with /dev/urandom */
  if (!seed_generated && getentropy(seeds, len) == 0) {
    fprintf(stderr, "%s Seeded RNG with getentropy()\n", time_string());
    seed_generated = true;
  }
#endif

#ifdef HAVE_ARC4RANDOM_BUF
  /* Most (all?) of the BSDs have this seeder. Use it for the reasons
     above. Also available on Linux with libbsd, but we don't check
     for that. */
  if (!seed_generated) {
    arc4random_buf(seeds, len);
    fprintf(stderr, "%s Seeded RNG with arc4random\n", time_string());
    seed_generated = true;
  }
#endif

#ifdef WIN32
  if (!seed_generated) {
    /* Use the Win32 bcrypto RNG interface */
    if (BCryptGenRandom(NULL, (PUCHAR) seeds, len,
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) == STATUS_SUCCESS) {
      fprintf(stderr, "%s Seeding RNG with BCryptGenRandom()\n", time_string());
      seed_generated = true;
    }
  }
#endif

#ifdef HAVE_DEV_URANDOM
  if (!seed_generated) {
    /* Seed from /dev/urandom if available */
    int fd;

    fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
      int r = read(fd, (void *) seeds, len);
      close(fd);
      if (r != len) {
        fprintf(stderr,
                "%s Couldn't read from /dev/urandom! Resorting to normal "
                "seeding method.\n",
                time_string());
      } else {
        fprintf(stderr, "%s Seeding RNG with /dev/urandom\n", time_string());
        seed_generated = true;
      }
    } else {
      fprintf(stderr,
              "%s Couldn't open /dev/urandom to seed random number "
              "generator. Resorting to normal seeding method.\n",
              time_string());
    }
  }
#endif

  if (!seed_generated) {
    /* Default seeder. Pick a seed that's slightly random */
#ifdef WIN32
    seeds[0] = (uint64_t) time(NULL);
    seeds[1] = (uint64_t) GetCurrentProcessId() + stream_count;
#else
    seeds[0] = (uint64_t) time(NULL);
    seeds[1] = (uint64_t) getpid() + stream_count;
#endif
    stream_count += 1;
  }
}

/** Initialize the SSL context.
 * \return pointer to SSL context object.
 */
SSL_CTX *
ssl_init(char *private_key_file, char *ca_file, char *ca_dir,
         int req_client_cert)
{
  const SSL_METHOD
    *meth; /* If this const gives you a warning, you're
              using an old version of OpenSSL. Walker, this means you! */
  /* uint8_t context[128]; */
  unsigned int reps = 1;
  pcg32_random_t rand_state;
  uint64_t seeds[2];
  bool seeded = false;

  if (!bio_err) {
    if (!SSL_library_init())
      return NULL;
    SSL_load_error_strings();
    /* Error write context */
    bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);
  }

  lock_file(stderr);
  fprintf(stderr, "%s Seeding OpenSSL random number pool.\n", time_string());
  unlock_file(stderr);
  while (!RAND_status()) {
    /* At this point, a system with /dev/urandom or a EGD file in the usual
       places will have enough entropy. Otherwise, be lazy and use random
       numbers until it's satisfied. */
    uint32_t gibberish[8];
    int n;

    if (!seeded) {
      generate_seed(seeds);
      pcg32_srandom_r(&rand_state, seeds[0], seeds[1]);
      seeded = 1;
    }

    for (n = 0; n < 8; n++)
      gibberish[n] = pcg32_random_r(&rand_state);

    RAND_seed(gibberish, sizeof gibberish);

    reps += 1;
  }

  lock_file(stderr);
  fprintf(stderr, "%s Seeded after %u %s.\n", time_string(), reps,
          reps > 1 ? "cycles" : "cycle");
  unlock_file(stderr);

  /* Set up SIGPIPE handler here? */

  /* Create context */
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  meth = TLS_server_method();
#else
  meth = SSLv23_server_method();
#endif
  ctx = SSL_CTX_new(meth);
  SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

  /* Load keys/certs */
  if (private_key_file && *private_key_file) {
    if (!SSL_CTX_use_certificate_chain_file(ctx, private_key_file)) {
      ssl_errordump("Unable to load server certificate - only anonymous "
                    "ciphers supported.");
    }
    if (!SSL_CTX_use_PrivateKey_file(ctx, private_key_file, SSL_FILETYPE_PEM)) {
      ssl_errordump(
        "Unable to load private key - only anonymous ciphers supported.");
    }
  }

  /* Load trusted CAs */
  if ((ca_file && *ca_file) || (ca_dir && *ca_dir)) {
    if (!SSL_CTX_load_verify_locations(ctx,
                                       (ca_file && *ca_file) ? ca_file : NULL,
                                       (ca_dir && *ca_dir) ? ca_dir : NULL)) {
      ssl_errordump("Unable to load CA certificates");
    }
    {
      STACK_OF(X509_NAME) *certs = NULL;
      if (ca_file && *ca_file)
        certs = SSL_load_client_CA_file(ca_file);
      if (certs)
        SSL_CTX_set_client_CA_list(ctx, certs);

      if (req_client_cert)
        SSL_CTX_set_verify(ctx,
                           SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                           client_verify_callback);
      else
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, client_verify_callback);
#if (OPENSSL_VERSION_NUMBER < 0x0090600fL)
      SSL_CTX_set_verify_depth(ctx, 1);
#endif
    }
  }

  SSL_CTX_set_options(ctx, SSL_OP_SINGLE_DH_USE | SSL_OP_ALL);
  SSL_CTX_set_mode(ctx, SSL_MODE_ENABLE_PARTIAL_WRITE |
                          SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

  /* Set up DH key */
  {
    DH *dh;
    dh = get_dh2048();
    SSL_CTX_set_tmp_dh(ctx, dh);
    DH_free(dh);
  }

#ifdef NID_X9_62_prime256v1
  /* Set up ECDH key */
  {
    EC_KEY *ecdh = NULL;
    ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    SSL_CTX_set_tmp_ecdh(ctx, ecdh);
    EC_KEY_free(ecdh);
  }
#endif

  /* Set the cipher list to the usual default list, except that
   * we'll allow anonymous diffie-hellman, too.
   */
  SSL_CTX_set_cipher_list(ctx, "ALL:ECDH:ADH:!LOW:!MEDIUM:@STRENGTH");

  /* Set up session cache if we can */
  /*
     strncpy((char *) context, MUDNAME, 128);
     SSL_CTX_set_session_id_context(ctx, context, strlen(context));
   */

  return ctx;
}

static int
client_verify_callback(int preverify_ok, X509_STORE_CTX *x509_ctx)
{
  char buf[256];
  X509 *err_cert;
  int err, depth;

  err_cert = X509_STORE_CTX_get_current_cert(x509_ctx);
  err = X509_STORE_CTX_get_error(x509_ctx);
  depth = X509_STORE_CTX_get_error_depth(x509_ctx);

  X509_NAME_oneline(X509_get_subject_name(err_cert), buf, 256);
  if (!preverify_ok) {
    lock_file(stderr);
    fprintf(stderr, "%s verify error:num=%d:%s:depth=%d:%s\n", time_string(),
            err, X509_verify_cert_error_string(err), depth, buf);
    if (err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT) {
      X509_NAME_oneline(
        X509_get_issuer_name(X509_STORE_CTX_get_current_cert(x509_ctx)), buf,
        256);
      fprintf(stderr, "%s issuer= %s\n", time_string(), buf);
    }
    unlock_file(stderr);
    return preverify_ok;
  }
  /* They've passed the preverification */
  /* if there are contents of the cert we wanted to verify, we'd do it here.
   */
  return preverify_ok;
}

static DH *
get_dh2048(void)
{
  static const uint8_t dh2048_p[] = {
    0x8C, 0x9A, 0x5A, 0x28, 0xBF, 0x13, 0x24, 0xC0, 0xD3, 0x7A, 0x73, 0xC3,
    0x87, 0x5B, 0x80, 0x81, 0xE8, 0xF3, 0x7B, 0xF6, 0xF7, 0x18, 0x71, 0xF9,
    0xBB, 0x5B, 0x88, 0x21, 0xAB, 0x63, 0xF6, 0x82, 0xA6, 0xEC, 0xD7, 0x04,
    0x25, 0xDC, 0x64, 0x75, 0x00, 0x49, 0x2C, 0x13, 0x04, 0x4F, 0xCF, 0xF9,
    0x06, 0xE0, 0x4D, 0x23, 0xB8, 0x7C, 0xD8, 0x29, 0x59, 0x6F, 0x69, 0xCC,
    0x41, 0x1F, 0x45, 0xF8, 0x25, 0xC8, 0x72, 0xF4, 0xC8, 0x37, 0x3C, 0x30,
    0xC2, 0x5A, 0xF3, 0x14, 0x43, 0x98, 0x4F, 0x99, 0x12, 0xBC, 0x68, 0x7E,
    0x20, 0x24, 0xAA, 0x8B, 0xBA, 0x87, 0x32, 0xBC, 0x4B, 0xF3, 0x16, 0x25,
    0xEE, 0xE5, 0xEB, 0x47, 0xED, 0xB2, 0x7D, 0x8F, 0x4F, 0xC8, 0xFB, 0x58,
    0x3D, 0x2E, 0xF6, 0x54, 0xF4, 0xDA, 0xD1, 0x88, 0x6A, 0xD8, 0xBC, 0x32,
    0xEC, 0xDA, 0xF1, 0xBC, 0xAF, 0x16, 0x90, 0xCD, 0xEE, 0x5F, 0x92, 0x0B,
    0xCE, 0xB9, 0x26, 0xCF, 0x18, 0xAE, 0x8C, 0x9B, 0x06, 0x0B, 0x83, 0x4D,
    0x99, 0x31, 0x98, 0x3B, 0x29, 0xE1, 0x16, 0x6A, 0xA4, 0x5E, 0xE8, 0x10,
    0x5F, 0x5B, 0x72, 0x3A, 0xA1, 0xD9, 0x89, 0x70, 0x61, 0xD9, 0xC2, 0x25,
    0x53, 0x5C, 0x44, 0x10, 0x27, 0xD7, 0xF2, 0x68, 0x75, 0x3F, 0xA3, 0xA7,
    0xCF, 0x02, 0x03, 0x49, 0xB4, 0xE4, 0xAF, 0x08, 0xEA, 0xAE, 0x97, 0x07,
    0x36, 0xC8, 0xD5, 0x24, 0xC6, 0x51, 0x8B, 0x91, 0x9A, 0x14, 0x91, 0x67,
    0x6A, 0xC0, 0xC3, 0x0E, 0x7C, 0xD8, 0x1F, 0xD2, 0x31, 0x07, 0x59, 0x5D,
    0x1D, 0xBD, 0x8E, 0xAE, 0xD7, 0x01, 0xBA, 0xDE, 0x0B, 0xDA, 0xA6, 0xBC,
    0x9A, 0xD1, 0x39, 0x59, 0x8F, 0xE5, 0x72, 0x65, 0x0F, 0x2A, 0x2D, 0x90,
    0x56, 0xE9, 0xDA, 0xF5, 0x4A, 0x26, 0xD3, 0xB3, 0x56, 0x19, 0x84, 0x00,
    0x3A, 0x11, 0x78, 0x83,
  };
  static const uint8_t dh2048_g[] = {
    0x02,
  };
  DH *dh;

  if ((dh = DH_new()) == NULL)
    return NULL;

#ifdef HAVE_DH_SET0_PQG
  BIGNUM *p, *g;
  p = BN_bin2bn(dh2048_p, sizeof(dh2048_p), NULL);
  if (!p) {
    lock_file(stderr);
    fprintf(stderr, "%s Error in BN_bin2bn 1!\n", time_string());
    unlock_file(stderr);
    DH_free(dh);
    return NULL;
  }

  g = BN_bin2bn(dh2048_g, sizeof(dh2048_g), NULL);
  if (!g) {
    lock_file(stderr);
    fprintf(stderr, "%s Error in BN_bin2bn 2!\n", time_string());
    unlock_file(stderr);
    BN_free(p);
    DH_free(dh);
    return NULL;
  }

  DH_set0_pqg(dh, p, NULL, g);
#else
  dh->p = BN_bin2bn(dh2048_p, sizeof(dh2048_p), NULL);
  if (!dh->p) {
    lock_file(stderr);
    fprintf(stderr, "%s Error in BN_bin2bn 1!\n", time_string());
    unlock_file(stderr);
    DH_free(dh);
    return NULL;
  }

  dh->g = BN_bin2bn(dh2048_g, sizeof(dh2048_g), NULL);
  if (!dh->g) {
    lock_file(stderr);
    fprintf(stderr, "%s Error in BN_bin2bn 2!\n", time_string());
    unlock_file(stderr);
    DH_free(dh);
    return NULL;
  }
#endif

  return dh;
}

SSL *
ssl_alloc_struct(void)
{
  return SSL_new(ctx);
}

/** Associate an SSL object with a socket and return it.
 * \param sock socket descriptor to associate with an SSL object.
 * \return pointer to SSL object.
 */
SSL *
ssl_setup_socket(int sock)
{
  SSL *ssl;
  BIO *bio;

  ssl = ssl_alloc_struct();
  bio = BIO_new_socket(sock, BIO_NOCLOSE);
  BIO_set_nbio(bio, 1);
  SSL_set_bio(ssl, bio, bio);
  return ssl;
}

/** Close down an SSL connection and free the object.
 * \param ssl pointer to SSL object to close down.
 * Technically, this function sends a shutdown notification
 * and then frees the object without waiting for acknowledgement
 * of the shutdown. If there were a good way to do that, it would
 * be better.
 */
void
ssl_close_connection(SSL *ssl)
{
  SSL_shutdown(ssl);
  SSL_free(ssl);
}

/** Given an accepted connection on the listening socket, set up SSL.
 * \param sock an accepted socket (returned by accept())
 * \param state pointer to place to return connection state.
 * \return an SSL object to associate with the listen end of this connection.
 */
SSL *
ssl_listen(int sock, int *state)
{
  SSL *ssl;
  ssl = ssl_setup_socket(sock);
  *state = ssl_accept(ssl);
  return ssl;
}

/** Given an accepted connection on the listening socket, resume SSL.
 * \param sock an accepted socket (returned by accept())
 * \param state pointer to place to return connection state.
 * \return an SSL object to associate with the listen end of this connection.
 */
SSL *
ssl_resume(int sock, int *state)
{
  SSL *ssl;

  ssl = ssl_setup_socket(sock);
  SSL_set_accept_state(ssl);
  *state = ssl_handshake(ssl);
  return ssl;
}

/** Perform an SSL handshake.
 * In some cases, a handshake may block, so we may have to call this
 * function again. Accordingly, we return state information that
 * tells us if we need to do that.
 * \param ssl pointer to an SSL object.
 * \return resulting state of the object.
 */
int
ssl_handshake(SSL *ssl)
{
  int ret;
  int state = 0;

  if ((ret = SSL_do_handshake(ssl)) <= 0) {
    switch (SSL_get_error(ssl, ret)) {
    case SSL_ERROR_WANT_READ:
      /* We must want for the socket to be readable, and then repeat
       * the call.
       */
      ssl_debugdump("SSL_do_handshake wants read");
      state = MYSSL_RB | MYSSL_HANDSHAKE;
      break;
    case SSL_ERROR_WANT_WRITE:
      /* We must want for the socket to be writable, and then repeat
       * the call.
       */
      ssl_debugdump("SSL_do_handshake wants write");
      state = MYSSL_WB | MYSSL_HANDSHAKE;
      break;
    default:
      /* Oops, don't know what's wrong */
      ssl_errordump("Error in ssl_handshake");
      state = -1;
    }
  } else {
    state = ssl_accept(ssl);
  }
  return state;
}

/** Given connection state, determine if an SSL_accept needs to be
 * performed. This is a just a wrapper so we don't have to expose
 * our internal state management stuff.
 * \param state an ssl connection state.
 * \return 0 if no ssl_accept is needed, non-zero otherwise.
 */
int
ssl_need_accept(int state)
{
  return (state & MYSSL_ACCEPT);
}

/** Given connection state, determine if an SSL_handshake needs to be
 * performed. This is a just a wrapper so we don't have to expose
 * our internal state management stuff.
 * \param state an ssl connection state.
 * \return 0 if no ssl_handshake is needed, non-zero otherwise.
 */
int
ssl_need_handshake(int state)
{
  return (state & MYSSL_HANDSHAKE);
}

/** Given connection state, determine if it's blocked on write.
 * This is a just a wrapper so we don't have to expose
 * our internal state management stuff.
 * \param state an ssl connection state.
 * \return 0 if no ssl_handshake is needed, non-zero otherwise.
 */
int
ssl_want_write(int state)
{
  return (state & MYSSL_WB);
}

/** Call SSL_accept and return the connection state.
 * \param ssl pointer to an SSL object.
 * \return ssl state flags indicating success, pending, or failure.
 */
int
ssl_accept(SSL *ssl)
{
  int ret;
  int state = 0;
  X509 *peer;
  char buf[256];

  if ((ret = SSL_accept(ssl)) <= 0) {
    switch (SSL_get_error(ssl, ret)) {
    case SSL_ERROR_WANT_READ:
      /* We must want for the socket to be readable, and then repeat
       * the call.
       */
      ssl_debugdump("SSL_accept wants read");
      state = MYSSL_RB | MYSSL_ACCEPT;
      break;
    case SSL_ERROR_WANT_WRITE:
      /* We must want for the socket to be writable, and then repeat
       * the call.
       */
      ssl_debugdump("SSL_accept wants write");
      state = MYSSL_WB | MYSSL_ACCEPT;
      break;
    default:
      /* Oops, don't know what's wrong */
      ssl_errordump("Error accepting connection");
      return -1;
    }
  } else {
    /* Successful accept - report it */
    if ((peer = SSL_get_peer_certificate(ssl))) {
      if (SSL_get_verify_result(ssl) == X509_V_OK) {
        /* The client sent a certificate which verified OK */
        X509_NAME_oneline(X509_get_subject_name(peer), buf, 256);
        lock_file(stderr);
        fprintf(stderr, "%s SSL client certificate accepted: %s", time_string(),
                buf);
        unlock_file(stderr);
        state |= MYSSL_VERIFIED;
      }
    }
  }

  return state;
}

/** Given an SSL object and its last known state, attempt to read from it.
 * \param ssl pointer to SSL object.
 * \param state saved state of SSL object.
 * \param net_read_ready 1 if the underlying socket is ready for read.
 * \param net_write_ready 1 if the underlying socket is ready for write.
 * \param buf buffer to read into.
 * \param bufsize number of bytes to read.
 * \param bytes_read pointer to return the number of bytes read.
 * \return new state of SSL object, or -1 if the connection closed.
 */
int
ssl_read(SSL *ssl, int state, int net_read_ready, int net_write_ready,
         char *buf, int bufsize, int *bytes_read)
{
  if ((net_read_ready && !(state & MYSSL_WBOR)) ||
      (net_write_ready && (state & MYSSL_RBOW))) {
    do {
      state &= ~(MYSSL_RB | MYSSL_RBOW);
      *bytes_read = SSL_read(ssl, buf, bufsize);
      switch (SSL_get_error(ssl, *bytes_read)) {
      case SSL_ERROR_NONE:
        /* Yay */
        return state;
      case SSL_ERROR_ZERO_RETURN:
        /* End of data on this socket */
        return -1;
      case SSL_ERROR_WANT_READ:
        /* More needs to be read from the underlying socket */
        ssl_debugdump("SSL_read wants read");
        state |= MYSSL_RB;
        break;
      case SSL_ERROR_WANT_WRITE:
        /* More needs to be written to the underlying socket.
         * This can happen during a rehandshake.
         */
        ssl_debugdump("SSL_read wants write");
        state |= MYSSL_RBOW;
        break;
      default:
        /* Should never happen */
        ssl_errordump("Unknown ssl_read failure!");
        return -1;
      }
    } while (SSL_pending(ssl) && !(state & MYSSL_RB));
  }
  return state;
}

/** Given an SSL object and its last known state, attempt to write to it.
 * \param ssl pointer to SSL object.
 * \param state saved state of SSL object.
 * \param net_read_ready 1 if the underlying socket is ready for read.
 * \param net_write_ready 1 if the underlying socket is ready for write.
 * \param buf buffer to write.
 * \param bufsize length of buffer to write.
 * \param offset pointer to offset into buffer marking where to write next.
 * \return new state of SSL object, or -1 if the connection closed.
 */
int
ssl_write(SSL *ssl, int state, int net_read_ready, int net_write_ready,
          const char *buf, int bufsize, int *offset)
{
  int r;
  if ((net_write_ready && bufsize) ||
      (net_read_ready && !(state & MYSSL_WBOR))) {
    state &= ~(MYSSL_WBOR | MYSSL_WB);
    r = SSL_write(ssl, buf + *offset, bufsize);
    switch (SSL_get_error(ssl, r)) {
    case SSL_ERROR_NONE:
      /* We wrote something, but maybe not all */
      *offset += r;
      break;
    case SSL_ERROR_WANT_WRITE:
      /* Underlying socket isn't ready to be written to. */
      ssl_debugdump("SSL_write wants write");
      state |= MYSSL_WB;
      break;
    case SSL_ERROR_WANT_READ:
      /* More needs to be read from the underlying socket first.
       * This can happen during a rehandshake.
       */
      ssl_debugdump("SSL_write wants read");
      state |= MYSSL_WBOR;
      break;
    default:
      /* Should never happen */
      ssl_errordump("Unknown ssl_write failure!");
    }
  }
  return state;
}

static void
ssl_errordump(const char *msg)
{
  lock_file(stderr);
  fprintf(stderr, "%s %s\n", time_string(), msg);
  ERR_print_errors(bio_err);
  unlock_file(stderr);
}
