/* passive_client.h */

#ifndef __PASSIVE_CLIENT_H
#define __PASSIVE_CLIENT_H

#include <neuro/NEURO.h>

#include "global.h"

extern void Passive_HandleData(const char *data, u32 len);

extern void Passive_SetScreenSize(int cols, int rows);

extern void Passive_Poll();

extern int Passive_Init(char *username, int layer);

extern void Passive_Clean();

#endif /* NOT __PASSIVE_CLIENT_H */
