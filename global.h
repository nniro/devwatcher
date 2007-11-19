/* global.h */

#ifndef __GLOBAL_H
#define __GLOBAL_H

#define NEURO_PROJECT_NAMESPACE "nsync"

enum packet_types
{
	NET_START,

	/* s2c and c2s */
	NET_HEADER,
	NET_DATA,
	NET_ALIVE,

	/* s2c */
	NET_LIST, /* send list */

	/* c2s */
	NET_CONNECT,
	NET_QLIST, /* query list */

	NET_END
};


typedef struct Pkt_Header Pkt_Header;

struct Pkt_Header
{
	u32 type; /* the packet type */
};


typedef struct Pkt_Connect Pkt_Connect;

struct Pkt_Connect
{
	char name[32];
	char password[32];
	u8 client_type;
	char version[8];
};

#endif /* NOT __GLOBAL_H */
