/* global.h */

#ifndef __GLOBAL_H
#define __GLOBAL_H

#define NEURO_PROJECT_NAMESPACE "nsync"

enum packet_types
{
	NET_START,

	NET_HEADER,

	/* s2c */

	/* c2s */
	NET_CONNECT,

	NET_END
};


typedef struct Pkt_Header Pkt_Header;

struct Pkt_Header
{
	u32 type; /* the packet type */
	u32 src_id; /* the client can't give that 
		     * information but it can use 
		     * it when recieved to know 
		     * who to reply. Only the server
		     * can fill that information. 
		     */
	u32 dest; /* the client can't fill that value
		   * but it can only do so when the src_id
		   * has a non NULL value... in that case,
		   * the client can put the src_id into that
		   * variable.
		   */
};


typedef struct Pkt_Connect Pkt_Connect;

struct Pkt_Connect
{
	char name[32];
	char version[8];
};

#endif /* NOT __GLOBAL_H */
