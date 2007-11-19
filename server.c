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
	u8 client_type; /* 0 is passive, 1 is active */
	char name[32];

	/* sessions (layers of active clients that a passive client can watch) */

};

typedef struct CList CList;

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

static LISTEN_DATA *network;

static EBUF *client_list;

static Packet *pktbuf;

/* main server password */
static char *server_password;

/* HACK hardcoding FIXME */ 
static CONNECT_DATA *hc_aclient;

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
	void *buffer;

        whole = (Pkt_Header*)data;

	buffer = &whole[1];

	/* NEURO_TRACE("Packet recieved type %d", whole->type); */

        switch (whole->type)
        {
		case NET_CONNECT:
		{
			Pkt_Connect *connect;
			CList *buf;

			connect = buffer;

			if (connect->client_type > 1)
			{
				printf("client has an unknown client type, dropping client\n");
				return 1;
			}

			if (server_password && connect->client_type == 1)
			{
				if (!strcmp(server_password, connect->password))
				{
					printf("client GRANTED ");
					printf("active access to broadcasting server\n");


					/* FIXME hardcode */
					hc_aclient = conn;
					/* FIXME end hardcode */
				}
				else
				{
					printf("client DENIED ");
					printf("active access to broadcasting server\n");
					return 1;
				}
			}

			Neuro_AllocEBuf(client_list, sizeof(CList*), sizeof(CList));

			buf = Neuro_GiveCurEBuf(client_list);

			buf->client = conn;
			buf->client_type = connect->client_type;

			if (connect->name)
			{
				strncpy(buf->name, connect->name, 32);
				printf("Connection from client %s type %d\n", buf->name, buf->client_type);
			}
		}
		break;

		case NET_ALIVE:
		{
			Packet_Reset(pktbuf);

			Packet_Push32(pktbuf, NET_ALIVE);

			NNet_Send(conn, Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));		
		}
		break;

		case NET_DATA:
		{
			int *tmp;
			char *bufa;

			tmp = buffer;
			bufa = (char*)&tmp[1];

			printf("%c%c", bufa[0], bufa[1]);
			/* FIXME hardcode */
			if (conn == hc_aclient)
			{
				u32 total = 0;
				CList *buf;

				if (Neuro_EBufIsEmpty(client_list))
					return 0;

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
					
					if (!buf->client_type)
					{
						/* printf("forwarding data len %d\n", len); */
						NNet_Send(buf->client, data, len);
					}
				}
			}
			/* FIXME end hardcode */
		}
		break;

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
Server_Init(char *password, int port)
{
	network = NNet_Create(packet_handler, 0);

	if(NNet_Listen(network, port))
	{
		NEURO_ERROR("failed to listen", NULL);
		return 1;
	}

	Neuro_CreateEBuf(&client_list);

	pktbuf = Packet_Create();

	if (password)
		server_password = password;

	return 0;
}

void
Server_Clean()
{
	Packet_Destroy(pktbuf);

	Neuro_CleanEBuf(&client_list);
	NNet_Destroy(network);
}

