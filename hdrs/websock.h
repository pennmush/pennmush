/*
 * WebSockets (RFC 6455) support.
 */

#ifndef MUSH_WEBSOCK_H
#define MUSH_WEBSOCK_H
#include "copyrite.h"
#include "function.h"

#define IsWebSocket(d) ((d)->conn_flags & CONN_WEBSOCKETS)

#define WEBSOCKET_CHANNEL_AUTO (0)
#define WEBSOCKET_CHANNEL_TEXT ('t')
#define WEBSOCKET_CHANNEL_JSON ('j')
#define WEBSOCKET_CHANNEL_HTML ('h')
#define WEBSOCKET_CHANNEL_PUEBLO ('p')
#define WEBSOCKET_CHANNEL_PROMPT ('>')

/* notify.c */
int queue_newwrite_channel(DESC *d, const char *b, int n, char ch);
int queue_newwrite(DESC *d, const char *b, int n);
int process_output(DESC *d);

/* websock.c */
int is_websocket(const char *command);
int process_websocket_request(DESC *d, const char *command);
int process_websocket_frame(DESC *d, char *tbuf1, int got);
void to_websocket_frame(const char **bp, int *np, char channel);

int markup_websocket(char *buff, char **bp, char *data, int datalen, char *alt,
                     int altlen, char channel);
void send_websocket_object(DESC *d, const char *header, cJSON *data);

FUNCTION_PROTO(fun_websocket_json);
FUNCTION_PROTO(fun_websocket_html);

#endif /* undef MUSH_WEBSOCK_H */
