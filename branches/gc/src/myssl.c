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
#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#ifdef I_SYS_TYPES
#include <sys/types.h>
#endif
#ifdef WIN32
#define FD_SETSIZE 256
#include <windows.h>
#include <winsock.h>
#include <io.h>
#define EWOULDBLOCK WSAEWOULDBLOCK
#define MAXHOSTNAMELEN 32
#define LC_MESSAGES 6
void shutdown_checkpoint(void);
#else                           /* !WIN32 */
#ifdef I_SYS_FILE
#include <sys/file.h>
#endif
#ifdef I_SYS_TIME
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif
#include <sys/ioctl.h>
#include <errno.h>
#ifdef I_SYS_SOCKET
#include <sys/socket.h>
#endif
#ifdef I_NETINET_IN
#include <netinet/in.h>
#endif
#ifdef I_NETDB
#include <netdb.h>
#endif
#ifdef I_SYS_PARAM
#include <sys/param.h>
#endif
#ifdef I_SYS_STAT
#include <sys/stat.h>
#endif
#endif                          /* !WIN32 */
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#ifdef I_SYS_SELECT
#include <sys/select.h>
#endif
#ifdef I_UNISTD
#include <unistd.h>
#endif
#include <stdio.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/dh.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include "conf.h"
#include "mysocket.h"
#include "externs.h"
#include "myssl.h"
#include "parse.h"
#include "SFMT.h"
#include "confmagic.h"

#define MYSSL_RB        0x1     /**< Read blocked (on read) */
#define MYSSL_WB        0x2     /**< Write blocked (on write) */
#define MYSSL_RBOW      0x4     /**< Read blocked (on write) */
#define MYSSL_WBOR      0x8     /**< Write blocked (on read) */
#define MYSSL_ACCEPT    0x10    /**< We need to call SSL_accept (again) */
#define MYSSL_VERIFIED  0x20    /**< This is an authenticated connection */
#define MYSSL_HANDSHAKE 0x40    /**< We need to call SSL_do_handshake */

#undef MYSSL_DEBUG
#ifdef MYSSL_DEBUG
#define ssl_debugdump(x) ssl_errordump(x)
#else
#define ssl_debugdump(x)
#endif

static void ssl_errordump(const char *msg);
static int client_verify_callback(int preverify_ok, X509_STORE_CTX *x509_ctx);
static DH *get_dh1024(void);


static BIO *bio_err = NULL;
static SSL_CTX *ctx = NULL;

/** Initialize the SSL context.
 * \return pointer to SSL context object.
 */
SSL_CTX *
ssl_init(char *private_key_file, char *ca_file, int req_client_cert)
{
  const SSL_METHOD *meth;       /* If this const gives you a warning, you're
                                   using an old version of OpenSSL. Walker, this means you! */
  /* unsigned char context[128]; */
  DH *dh;
  unsigned int reps = 1;

  if (!bio_err) {
    if (!SSL_library_init())
      return NULL;
    SSL_load_error_strings();
    /* Error write context */
    bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);
  }

  lock_file(stderr);
  fputs("Seeding OpenSSL random number pool.\n", stderr);
  unlock_file(stderr);
  while (!RAND_status()) {
    /* At this point, a system with /dev/urandom or a EGD file in the usual
       places will have enough entropy. Otherwise, be lazy and use random numbers
       until it's satisfied. */
    uint32_t gibberish[4];
    int n;

    for (n = 0; n < 4; n++)
      gibberish[n] = gen_rand32();

    RAND_seed(gibberish, sizeof gibberish);

    reps += 1;
  }

  lock_file(stderr);
  fprintf(stderr, "Seeded after %u %s.\n", reps, reps > 1 ? "cycles" : "cycle");
  unlock_file(stderr);

  /* Set up SIGPIPE handler here? */

  /* Create context */
  meth = SSLv23_server_method();
  ctx = SSL_CTX_new(meth);

  /* Load keys/certs */
  if (private_key_file && *private_key_file) {
    if (!SSL_CTX_use_certificate_chain_file(ctx, private_key_file)) {
      ssl_errordump
        ("Unable to load server certificate - only anonymous ciphers supported.");
    }
    if (!SSL_CTX_use_PrivateKey_file(ctx, private_key_file, SSL_FILETYPE_PEM)) {
      ssl_errordump
        ("Unable to load private key - only anonymous ciphers supported.");
    }
  }

  /* Load trusted CAs */
  if (ca_file && *ca_file) {
    if (!SSL_CTX_load_verify_locations(ctx, ca_file, NULL)) {
      ssl_errordump("Unable to load CA certificates");
    } else {
      if (req_client_cert)
        SSL_CTX_set_verify(ctx,
                           SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                           client_verify_callback);
      else
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, client_verify_callback);
#if (OPENSSL_VERSION_NUMBER < 0x0090600fL)
      SSL_CTX_set_verify_depth(ctx, 1);
#endif
    }
  }

  SSL_CTX_set_options(ctx, SSL_OP_SINGLE_DH_USE | SSL_OP_ALL);
  SSL_CTX_set_mode(ctx,
                   SSL_MODE_ENABLE_PARTIAL_WRITE |
                   SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

  /* Set up DH callback */
  dh = get_dh1024();
  SSL_CTX_set_tmp_dh(ctx, dh);
  /* The above function makes a private copy of this */
  DH_free(dh);

  /* Set the cipher list to the usual default list, except that
   * we'll allow anonymous diffie-hellman, too.
   */
  SSL_CTX_set_cipher_list(ctx, "ALL:ADH:RC4+RSA:+SSLv2:@STRENGTH");

  /* Set up session cache if we can */
  /*
     strncpy((char *) context, MUDNAME, 128);
     SSL_CTX_set_session_id_context(ctx, context, u_strlen(context));
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
    fprintf(stderr, "verify error:num=%d:%s:depth=%d:%s\n", err,
            X509_verify_cert_error_string(err), depth, buf);
    if (err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT) {
      X509_NAME_oneline(X509_get_issuer_name(x509_ctx->current_cert), buf, 256);
      fprintf(stderr, "issuer= %s\n", buf);
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
get_dh1024(void)
{
  static unsigned char dh1024_p[] = {
    0xB6, 0xBC, 0x30, 0x5B, 0xB4, 0xE5, 0x96, 0x62, 0x3F, 0x85, 0x5B, 0x1F,
    0x88, 0xD1, 0x12, 0xE1, 0x1D, 0x27, 0x69, 0x63, 0xAD, 0xB3, 0x4D, 0x23,
    0xB8, 0x4B, 0x1A, 0x90, 0xA6, 0x89, 0xD8, 0x5D, 0xFA, 0xF5, 0x8F, 0xFF,
    0xFF, 0xF4, 0x54, 0x3B, 0xCD, 0x5C, 0xAA, 0x79, 0x8B, 0x14, 0xBB, 0x84,
    0xAC, 0xEE, 0x94, 0x47, 0x76, 0xEC, 0x46, 0x75, 0x26, 0x48, 0x8C, 0x06,
    0x55, 0x27, 0x7F, 0xC0, 0xF1, 0xE8, 0x1F, 0xD2, 0xE4, 0x55, 0xAE, 0x78,
    0x11, 0x6E, 0xF1, 0x3B, 0xCD, 0x55, 0xE8, 0x17, 0xE9, 0x15, 0x7B, 0x05,
    0x91, 0x28, 0x9D, 0xD3, 0x40, 0x2E, 0x34, 0x03, 0x04, 0x2B, 0x2D, 0xC5,
    0x5C, 0x67, 0xC5, 0xF4, 0x28, 0x8E, 0x16, 0xAC, 0xDD, 0x68, 0x43, 0x66,
    0x51, 0xC1, 0x6F, 0x54, 0xB9, 0x22, 0xD8, 0x1A, 0x39, 0x6B, 0x0A, 0xC1,
    0x20, 0x5A, 0x9D, 0x31, 0x30, 0xE4, 0x0B, 0xC3,
  };
  static unsigned char dh1024_g[] = {
    0x02,
  };
  DH *dh;
  if ((dh = DH_new()) == NULL)
    return NULL;
  if (dh->p) {
    BN_free(dh->p);
    dh->p = NULL;
  }
  if (dh->g) {
    BN_free(dh->g);
    dh->g = NULL;
  }

  dh->p = BN_bin2bn(dh1024_p, sizeof(dh1024_p), NULL);
  if (!dh->p) {
    lock_file(stderr);
    fputs("Error in BN_bin2bn 1!\n", stderr);
    unlock_file(stderr);
    DH_free(dh);
    return NULL;
  }

  dh->g = BN_bin2bn(dh1024_g, sizeof(dh1024_g), NULL);
  if (!dh->g) {
    lock_file(stderr);
    fputs("Error in BN_bin2bn 2!\n", stderr);
    unlock_file(stderr);
    DH_free(dh);
    return NULL;
  }

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
        fprintf(stderr, "SSL client certificate accepted: %s", buf);
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
          unsigned char *buf, int bufsize, int *offset)
{
  int r;
  if ((net_write_ready && bufsize) || (net_read_ready && !(state & MYSSL_WBOR))) {
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
  fputs(msg, stderr);
  fputc('\n', stderr);
  ERR_print_errors(bio_err);
  unlock_file(stderr);
}
