/* client.c
 * Module : Client_
 */

/*-------------------- Extern Headers Including --------------------*/

#include <neuro/NEURO.h>
#include <neuro/nnet/network.h>
#include <string.h> /* strlen() */

/*-------------------- Local Headers Including ---------------------*/

#include "core.h"
#include "packet.h"

/*-------------------- Main Module Header --------------------------*/

#include "client.h"

/*--------------------      Other       ----------------------------*/

NEURO_MODULE_CHANNEL("client");

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

static LISTEN_DATA *network;
static CONNECT_DATA *client;

static Packet *pktbuf;

/*-------------------- Static Prototypes ---------------------------*/

/*-------------------- Static Functions ----------------------------*/

/*-------------------- Global Functions ----------------------------*/

/*-------------------- Poll -------------------------*/

void
Client_Poll()
{

}

/*-------------------- Packet handler Poll -------------------------*/

static int
packet_handler(CONNECT_DATA *conn, char *data, u32 len)
{
	Pkt_Header *whole;

	whole = (Pkt_Header*)data;



	switch (whole->type)
	{	
		default:
		{
			NEURO_WARN("Unhandled packet type recieved -- type %d", whole->type);
		}
		break;
	}

	Packet_Reset(pktbuf);

	return 0;
}

/*-------------------- Constructor Destructor ----------------------*/

int
Client_Init(char *username, char *host, int port)
{
	network = NNet_Create(packet_handler, 1);
	if(NNet_Connect(network, host, port, &client))
	{
                NEURO_ERROR("failed to connect", NULL);
                return 1;
	}

	pktbuf = Packet_Create();

	/* SendConnect(client, username); */

	return 0;
}

void
Client_Clean()
{
	Packet_Destroy(pktbuf);

        NNet_Destroy(network);	
}
