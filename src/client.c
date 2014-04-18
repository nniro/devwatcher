/* client.c
 * Module : Client_
 *
 * Need to create a function which will make all the thread close
 * down. This can then be used on error or what not to make sure
 * there are no left forks running as "zombies".
 */

/*-------------------- Extern Headers Including --------------------*/

#include <neuro/NEURO.h>
#include <neuro/network.h>
#include <neuro/packet.h>

#include <signal.h> /* signal */

#include <stdlib.h>
#include <stdio.h> /* printf */
#include <string.h> /* memcpy */

/*-------------------- Local Headers Including ---------------------*/

#include "main.h" /* Main_Exit */

#include "core.h"
#include "pktAsm.h"

#include "passive_client.h"
#include "active_client.h"

/*-------------------- Main Module Header --------------------------*/

#include "client.h"

/*--------------------      Other       ----------------------------*/

NEURO_MODULE_CHANNEL("client");

#define ALIVE_TIME 300

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

static NNET_SLAVE *client;

static Packet *pktbuf;

static PktAsm *pktAsmCtx;

/* time left until we send an alive packet type */
static t_tick alive_time; 

static int client_active = 0;

/*-------------------- Static Prototypes ---------------------------*/

static int packet_handler(NNET_SLAVE *conn, const char *data, u32 len);

/*-------------------- Static Functions ----------------------------*/

static void
clean_program(int dummy)
{
	Main_Exit();
}

static int
packetGetSize(const char *data)
{
	Pkt_Header *whole;

        whole = (Pkt_Header*)data;

	return whole->size;
}

/*-------------------- Global Functions ----------------------------*/

void
Client_SendPacket(char *data, u32 len)
{
	Pkt_Header *buf = (Pkt_Header*)data;

	memcpy(&buf->size, &len, sizeof(u32));

	TRACE(Neuro_s("Sending Packet : len %d ", len));
	NNet_Send(client, data, len);
}

/*-------------------- Poll -------------------------*/

int
Client_Poll(NNET_STATUS *status)
{
	u32 _err = 0;
	switch (NNet_GetStatus(status))
	{
		case State_NoData:
		{
			/* NEURO_TRACE("No data", NULL); */
		}
		break;

		case State_Disconnect:
		{
			TRACE("Internal Quit flagged, leaving");
			return 1;
		}

		case State_ClientDisconnect:
		{
			TRACE("Disconnect order by the server, leaving");
			/* closing the server */
			return 1;
		}
		break;

		case State_DataAvail:
		{
			_err = packet_handler(NNet_GetSlave(status), NNet_GetPacket(status), NNet_GetPacketLen(status));
		}
		break;

		default:
		{
			ERROR(Neuro_s("unknown status %d", NNet_GetStatus(status)));
			return 1;
		}
		break;
	}

	if (ACTIVE_CLIENT_ENABLED && client_active == 1)
		Active_Poll();

	if (alive_time < Neuro_GetTickCount())
	{
		alive_time = Neuro_GetTickCount() + ALIVE_TIME;
		Packet_Reset(pktbuf);

		Packet_Push32(pktbuf, NET_ALIVE);
		Packet_Push32(pktbuf, 0);

		Client_SendPacket(Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));
	}
	return _err;
}

/*-------------------- Packet handler Poll -------------------------*/

static int
packet_handler(NNET_SLAVE *conn, const char *data, u32 len)
{
	Pkt_Header *whole;
	void *buffer;

	char *newData = NULL;
	int newLen = 0;
	char *nextData = NULL;
	int nextLen = 0;
	int handleNextData = 0;

	/* NEURO_TRACE("recieved packet len %d", len); */

	if (len <= 0 || !data)
	{
		WARN("Invalid packet recieved");
		return 1;
	}

	{
		int _err = 0;

		_err = PktAsm_Process(pktAsmCtx, packetGetSize, (char*)data, len, &newData, &newLen, &nextData, &nextLen);

		switch (_err)
		{
			case 0:
			{
				/* needs more data */
				return 0;
			}
			break;

			case 1:
			{
				/* successfully scavenged a new packet */
				whole = (Pkt_Header*)newData;
				len = newLen;
				PktAsm_Reset(pktAsmCtx);
			}
			break;

			case 2:
			{
				/* successfully scavenged a new packet and got more data available */
				handleNextData = 1;
				whole = (Pkt_Header*)newData;
				len = newLen;
				PktAsm_Reset(pktAsmCtx);
			}
			break;

			case 3:
			{
				/* could not do anything with the data. */
			}
			break;

			default:
			{
				ERROR("PktAsm_Process : unrecoverable error");
				return 1;
			}
			break;
		}
	}


	buffer = &whole[1];

	switch (whole->type)
	{
		case NET_DATA:
		{
			/* we remove the length of the header variables */
			Passive_HandleData(buffer, whole->size - 8);
		}
		break;

		case NET_INFO:
		{
			Pkt_Info *tmp;
			int cols = 0, rows = 0;

			TRACE("NET_INFO packet received");

			tmp = (Pkt_Info*)buffer;

			if (tmp->access == 0)
			{
				TRACE("Server access denied");
				return 1;
			}

			if (newLen >= sizeof(Pkt_WSize))
			{
				cols = tmp->cols;
				rows = tmp->rows;
			}
			else
			{
				/* we disconnect */
				return 1;
			}

			if (client_active == 1)
			{
				TRACE("Starting Active Session");
				Active_SendWSize();
				Active_StartSession();
			}
			else
			{
				Passive_SetScreenSize(cols, rows);
			}
		}

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

			printf(Neuro_s("Active client %s -- Session Layer #%d\n", buf->name, buf->layers));
		}
		break;

		default:
		{
			WARN(Neuro_s("Unhandled packet type recieved -- type %d", whole->type));
		}
		break;
	}

	Packet_Reset(pktbuf);

	if (handleNextData)
	{
		return packet_handler(conn, nextData, nextLen);
	}

	return 0;
}

/*-------------------- Constructor Destructor ----------------------*/

int
Client_Init(NNET_MASTER *master, char *username, char *password, char *host, int port, int layer, int client_type)
{
	client = NNet_Connect(master, host, port);
	if(!client)
	{
                ERROR("failed to connect");
                return 1;
	}

	/*NNet_SetSendPacketSize(master);*/
	NNet_SetTimeOut(client, 0);

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
	pktAsmCtx = PktAsm_Create();

	return 0;
}

void
Client_Clean()
{
	Packet_Destroy(pktbuf);
	PktAsm_Destroy(pktAsmCtx);

	if (client_active == 1)
		Active_Clean();
	else
		Passive_Clean();
}
