/** ssl_slave API prototypes */

#ifndef SSL_SLAVE_H
#define SSL_SLAVE_H

enum ssl_slave_state { SSL_SLAVE_DOWN, SSL_SLAVE_RUNNING };

extern pid_t ssl_slave_pid;
extern enum ssl_slave_state ssl_slave_state;
extern int ssl_slave_ctl_fd;

int make_ssl_slave(void);
void kill_ssl_slave(void);

struct ssl_slave_config {
  char socket_file[FILE_PATH_LEN];
  char ssl_ip_addr[64];
  int normal_port;
  int ssl_port;
  int websock_port;
  char private_key_file[FILE_PATH_LEN];
  char ca_file[FILE_PATH_LEN];
  char ca_dir[FILE_PATH_LEN];
  int require_client_cert;
  int keepalive_timeout;
};

#endif
