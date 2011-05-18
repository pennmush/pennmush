/** ssl_slave API prototypes */

#ifndef SSL_SLAVE_H
#define SSL_SLAVE_H

enum ssl_slave_state {
  SSL_SLAVE_DOWN,
  SSL_SLAVE_RUNNING
};

extern pid_t ssl_slave_pid;
extern enum ssl_slave_state ssl_slave_state;

int make_ssl_slave(void);
void kill_ssl_slave(void);

#endif
