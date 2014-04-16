/* server.h */

#ifndef __SERVER_H
#define __SERVER_H

#include <neuro/NEURO.h>
#include <neuro/nnet/network.h>

#include "global.h"

extern int Server_Poll(NNET_STATUS *status);
extern int Server_Init(NNET_MASTER *master, char *password, int port);
extern void Server_Clean();

#endif /* NOT __SERVER_H */
