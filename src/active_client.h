/* active_client.h */

#ifndef __ACTIVE_CLIENT_H
#define __ACTIVE_CLIENT_H

#include <neuro/NEURO.h>

#include "global.h"

extern void Active_Poll();
extern int Active_Init(char *username, char *password);

/* starts the new shell and all */
extern int Active_StartSession();

/* send the screen size information */
extern void Active_SendWSize();

extern void Active_Clean();

#endif /* NOT __ACIVE_CLIENT_H */
