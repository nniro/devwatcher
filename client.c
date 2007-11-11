/* client.c
 * Module : Client_
 */

/*-------------------- Extern Headers Including --------------------*/

#include <neuro/NEURO.h>
#include <neuro/nnet/network.h>
#include <string.h> /* strlen() */

#include <stdio.h> /* printf() */
#include <unistd.h> /* execl() */

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

void
doshell()
{
	printf("entering a new shell, you can exit it at any time to stop this process\n");
	
	execl("/bin/bash", "-i", NULL);
}

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
Client_Init(char *username, char *password, char *host, int port, int client_type)
{
	network = NNet_Create(packet_handler, 1);
	if(NNet_Connect(network, host, port, &client))
	{
                NEURO_ERROR("failed to connect", NULL);
                return 1;
	}

	pktbuf = Packet_Create();

	if (client_type == 1)
	{
		doshell();
	}

	/* SendConnect(client, username); */

	return 0;
}

void
Client_Clean()
{
	Packet_Destroy(pktbuf);

        NNet_Destroy(network);	
}
