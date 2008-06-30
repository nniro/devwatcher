/* client.c
 * Module : Client_
 *
 */

/*-------------------- Extern Headers Including --------------------*/

#include <neuro/NEURO.h>
#include <neuro/nnet/network.h>

#include <signal.h> /* signal */

#include <stdio.h> /* printf */

/*-------------------- Local Headers Including ---------------------*/

#include "main.h" /* Main_Exit */

#include "core.h"
#include "packet.h"

#include "passive_client.h"
#include "active_client.h"

/*-------------------- Main Module Header --------------------------*/

#include "client.h"

/*--------------------      Other       ----------------------------*/

NEURO_MODULE_CHANNEL("client");

#define ALIVE_TIME 300

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

static LISTEN_DATA *network;
static CONNECT_DATA *client;

static Packet *pktbuf;

/* time left until we send an alive packet type */
static t_tick alive_time; 

static int client_active = 0;

/*-------------------- Static Prototypes ---------------------------*/

/*-------------------- Static Functions ----------------------------*/

static void
clean_program(int dummy)
{
	Main_Exit();
}

/*-------------------- Global Functions ----------------------------*/

void
Client_SendPacket(const char *data, u32 len)
{
	/* NEURO_TRACE("sending %s", Neuro_s("packet len %d \'%s\'", len, data)); */
	NNet_Send(client, data, len);
}

/* 
 * return 1 if the packet is valid and 0 if not 
 */
int
Client_IsValidPacket(const char *data, u32 len)
{
	Pkt_Header *whole;
	void *buffer;

	whole = (Pkt_Header*)data;

	buffer = &whole[1];

	switch (whole->type)
	{
		case NET_DATA:
		{
			int *length;
			int total = 0;
			char *buf = NULL;

			length = buffer;
			buf = (char*)&length[1];

			total = *length;
			total += 4 + 4;

			if ((len - total) >= total)
			{
				return Client_IsValidPacket(&buf[*length], len - total);
			}
		}
		break;

		case NET_ALIVE:
		{
		}
		break;

		case NET_DISCONNECT:
		{
		}
		break;

		case NET_LIST:
		{
		}
		break;

		default:
		{
			return 0;
		}
		break;
	}

	return 1;
}


/*-------------------- Poll -------------------------*/

void
Client_Poll()
{

	if (ACTIVE_CLIENT_ENABLED && client_active == 1)
		Active_Poll();

	if (alive_time < Neuro_GetTickCount())
	{
		alive_time = Neuro_GetTickCount() + ALIVE_TIME;
		Packet_Reset(pktbuf);

		Packet_Push32(pktbuf, NET_ALIVE);

		NNet_Send(client, Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));
	}
}

/*-------------------- Packet handler Poll -------------------------*/

static int
packet_handler(CONNECT_DATA *conn, const char *data, u32 len)
{
	Pkt_Header *whole;
	void *buffer;

	NEURO_TRACE("recieved packet len %d", len);

	whole = (Pkt_Header*)data;

	buffer = &whole[1];

	switch (whole->type)
	{
		case NET_DATA:
		{
			char *buf;
			int *length;
			int total = 0;

			if (Client_IsValidPacket(data, len) == 0)
			{
				NEURO_WARN("Invalid packet recieved", NULL);
			}

			length = buffer;
			buf = (char*)&length[1];

			total = *length;
			total += 4 + 4;

			Passive_HandleData(buf, *length);
		}
		break;

		case NET_ALIVE:
		{

		}
		break;

		case NET_DISCONNECT:
		{
			printf("Disconnection packet recieved from server... bailing out\n");
			return 1;
		}
		break;

		case NET_LIST:
		{
			Pkt_List *buf;

			buf = buffer;

			printf("Active client %s -- Sessions %d\n", buf->name, buf->layers);
		}
		break;

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
Client_Init(char *username, char *password, char *host, int port, int layer, int client_type)
{
	network = NNet_Create(packet_handler, 1);

	if(NNet_Connect(network, host, port, &client))
	{
                NEURO_ERROR("failed to connect", NULL);
                return 1;
	}

	NNet_ClientTogglePacketSize(client);
	NNet_SetTimeout(client, 0);

	if (client_type == 0)
	{
		Passive_Init(username, layer);
	}
	else
	{
		if (ACTIVE_CLIENT_ENABLED)
		{
			Active_Init(username, password);
			client_active = 1;
		}
	}

	
	signal(SIGINT, clean_program);


	/* NNet_SetTimeout(client, 0); */

	pktbuf = Packet_Create();

	return 0;
}

void
Client_Clean()
{
	Packet_Destroy(pktbuf);

        NNet_Destroy(network);

	if (client_active == 1)
		Active_Clean();
	else
		Passive_Clean();
}
