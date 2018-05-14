/** \file connlog.h
 *
 * \brief Interface for connlog functions.
 */

#pragma once

bool init_conndb(bool rebooting);
void shutdown_conndb(bool rebooting);
int64_t connlog_connection(const char *ip, const char *host);
void connlog_login(int64_t id, dbref player);
void connlog_disconnection(int64_t id, const char *reason);
