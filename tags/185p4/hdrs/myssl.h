/**
 * \file myssl.h
 *
 * \brief Code to support SSL connections
 */

#ifndef _MYSSL_H
#define _MYSSL_H

#include "copyrite.h"

SSL_CTX *ssl_init(char *private_key_file, char *ca_file, int req_client_cert);
SSL *ssl_setup_socket(int sock);
void ssl_close_connection(SSL *ssl);
SSL *ssl_alloc_struct(void);
SSL *ssl_listen(int sock, int *state);
SSL *ssl_resume(int sock, int *state);
int ssl_accept(SSL *ssl);
int ssl_handshake(SSL *ssl);
int ssl_need_accept(int state);
int ssl_need_handshake(int state);
int ssl_want_write(int state);
int ssl_read(SSL *ssl, int state, int net_read_ready, int net_write_ready,
             char *buf, int bufsize, int *bytes_read);
int ssl_write(SSL *ssl, int state, int net_read_ready, int net_write_ready,
              unsigned char *buf, int bufsize, int *offset);
void ssl_write_session(FILE * fp, SSL *ssl);
void ssl_read_session(FILE * fp);
void ssl_write_ssl(FILE * fp, SSL *ssl);
SSL *ssl_read_ssl(FILE * fp, int sock);

#endif                          /* _MYSSL_H */
