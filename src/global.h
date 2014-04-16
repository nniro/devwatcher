/* global.h 
 *
 */

#ifndef __GLOBAL_H
#define __GLOBAL_H

#define NEURO_PROJECT_NAMESPACE "dwatcher"

enum packet_types
{
	NET_START,

	/* s2c and c2s */
	NET_HEADER,
	NET_DATA,
	NET_ALIVE,

	/* s2c */
	NET_LIST, /* send list */
	NET_INFO, /* replies to an active or a passive client */
	NET_DISCONNECT, /* disconnects a client */

	/* c2s */
	NET_CONNECT,
	NET_WSIZE, /* active client to send the size of the screen */
	NET_QLIST, /* query list */
	NET_SELECT, /* select which element in the list to listen to. */

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
	u32 layer; /* for passive clients only */
	char password[32]; /* for active clients only */
	u8 client_type;
	char version[8];
};

typedef struct Pkt_Info Pkt_Info;

struct Pkt_Info
{
	int access; /* value of 1 if the client has access and 0 for otherwise (active & passive) */
	int cols, rows; /* sent to passive clients, the columns and rows of the transmission */
};

typedef struct Pkt_Select Pkt_Select;

struct Pkt_Select
{
	char name[32];
	u32 layer;
};

typedef struct Pkt_WSize Pkt_WSize;

struct Pkt_WSize
{
	int cols, rows;
};

typedef struct Pkt_List Pkt_List;

struct Pkt_List
{
	char name[32];
	u32 layers; /* the amount of layers */
	struct
	{
		u32 listeners; /* the amount of listeners */

		u32 summary_len;
		char *summary;

		u32 description_len;
		char *description;
	}layer;
};

#endif /* NOT __GLOBAL_H */
