/* main.h */

#ifndef __MAIN_H
#define __MAIN_H

#include "global.h"

/* makes the program stop running */
extern void Main_Exit();

/* returns the name of the client if any or NULL */
extern char *Main_GetClientName();
/* returns the name of the server to connect to or NULL */
extern char *Main_GetClientConnect();
/* returns a non negative value that is the port number 
 * for starting the server or for the client to connect to.
 */
extern int Main_GetPort();

extern char *Main_GetPassword();


extern int Main_GetClientType();

#endif /* NOT __MAIN_H */
