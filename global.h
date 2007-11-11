/* global.h */

#ifndef __GLOBAL_H
#define __GLOBAL_H

#define NEURO_PROJECT_NAMESPACE "nsync"

enum packet_types
{
	NET_START,

	NET_HEADER,

	NET_DATA,

	/* s2c */

	/* c2s */
	NET_CONNECT,

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
	char version[8];
};

#endif /* NOT __GLOBAL_H */
