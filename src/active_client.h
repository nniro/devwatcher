/* active_client.h */

#ifndef __ACTIVE_CLIENT_H
#define __ACTIVE_CLIENT_H

#include <neuro/NEURO.h>

#include "global.h"

extern void Active_Poll();
extern int Active_Init(char *username, char *password);

extern void Active_Clean();

#endif /* NOT __ACIVE_CLIENT_H */
