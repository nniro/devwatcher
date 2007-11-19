/* server.h */

#ifndef __SERVER_H
#define __SERVER_H

#include <neuro/NEURO.h>
#include <neuro/nnet/network.h>

#include "global.h"

extern void Server_Poll();
extern int Server_Init(char *password, int port);
extern void Server_Clean();

#endif /* NOT __SERVER_H */
