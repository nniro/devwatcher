/* server.c
 * Module : Server_
 */

/*-------------------- Extern Headers Including --------------------*/

#include <neuro/NEURO.h>
#include <neuro/nnet/network.h>
#include <string.h>

/*-------------------- Local Headers Including ---------------------*/

#include "core.h"
#include "packet.h"

/*-------------------- Main Module Header --------------------------*/

#include "server.h"

/*--------------------      Other       ----------------------------*/

NEURO_MODULE_CHANNEL("server");

struct CList
{
	CONNECT_DATA *client;
};

typedef struct CList CList;

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

static LISTEN_DATA *network;

static EBUF *client_list;

static Packet *pktbuf;

/*-------------------- Static Prototypes ---------------------------*/

/*-------------------- Static Functions ----------------------------*/

/*-------------------- Global Functions ----------------------------*/

/*-------------------- Poll ------------------------*/

void
Server_Poll()
{
	u32 total = 0;
	CList *buf;

	if (Neuro_EBufIsEmpty(client_list))
		return;

	total = Neuro_GiveEBufCount(client_list) + 1;

        while (total-- > 0)
        {
		buf = Neuro_GiveEBuf(client_list, total);

		if (NNet_ClientExist2(network, buf->client) == 0)
		{
			NEURO_TRACE("Disconnecting a zombie connection's data", NULL);
			Neuro_SCleanEBuf(client_list, buf);

			continue;
		}
	
        }
}

/*-------------------- Packets handler Poll ------------------------*/

static int
packet_handler(CONNECT_DATA *conn, char *data, u32 len)
{
        Pkt_Header *whole;

        whole = (Pkt_Header*)data;

	/* NEURO_TRACE("Packet recieved type %d", whole->type); */

        switch (whole->type)
        {

                default:
                {
                        NEURO_WARN("Unhandled packet type recieved", NULL);
                }
                break;
	}

	Packet_Reset(pktbuf);

	return 0;
}

/*-------------------- Constructor Destructor ----------------------*/

int
Server_Init(int port)
{
	network = NNet_Create(packet_handler, 0);

	if(NNet_Listen(network, port))
	{
		NEURO_ERROR("failed to listen", NULL);
		return 1;
	}

	Neuro_CreateEBuf(&client_list);

	pktbuf = Packet_Create();

	return 0;
}

void
Server_Clean()
{
	Packet_Destroy(pktbuf);

	Neuro_CleanEBuf(&client_list);
	NNet_Destroy(network);
}

