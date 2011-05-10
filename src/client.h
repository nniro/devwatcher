/* client.h */

#ifndef __CLIENT_H
#define __CLIENT_H

#include <neuro/NEURO.h>
#include <neuro/network.h>

#include "global.h"

extern void Client_SendPacket(const char *data, u32 len);

extern int Client_IsValidPacket(const char *data, u32 len);

extern int Client_Poll(NNET_STATUS *status);
extern int Client_Init(NNET_MASTER *master, char *username, char *password, char *host, int port, int layer, int client_type);

extern void Client_Clean();

#endif /* NOT __CLIENT_H */
