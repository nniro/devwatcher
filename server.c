/* server.c
 * Module : Server_
 *
 * the server module of the project
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

typedef struct CList CList;

struct CList
{
	u8 client_type; /* 0 is passive, 1 is active */
	char name[32];

	CONNECT_DATA *client; /* for active and passive clients, this is the pointer to the connection */
	CONNECT_DATA *audit; /* for passive clients, this is the pointer to an active connection. */

	/* sessions (layers of active clients that a passive client can watch) */
	EBUF *sessions; /* contains Session type of elements */

};

typedef struct Session Session;

struct Session
{
	/* each session need statistics like buffer size, how much bytes transfered 
	 * and how long since the last change. All this for the list query packet.
	 */

	char *summary;
	char *description;

	CONNECT_DATA *session;

	/* list of passive clients that listen that session */
	EBUF *listeners; /* contains Listener elements */
};

typedef struct Listener Listener;

struct Listener
{
	CONNECT_DATA *client;
};

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

static LISTEN_DATA *network;

static EBUF *client_list;

static Packet *pktbuf;

/* main server password */
static char *server_password;

#if hardcode
/* HACK hardcoding FIXME */ 
static CONNECT_DATA *hc_aclient;
#endif /* hardcode */

/*-------------------- Static Prototypes ---------------------------*/

/*-------------------- Static Functions ----------------------------*/

/* piece of code that checks if an element still exists or not then calls 
 * the necessary instructions to effectively clean the data.
 *
 * FIXME : when an active layer (active-layer) is cleaned off, we also need to
 * loop passive connections that are auditing it to make them disconnect nicely.
 */
static int
clist_check_zombie(CList *elem)
{

	if (elem->client_type == 1)
	{
		Session *buf;
		u32 total = 0;

		if (Neuro_EBufIsEmpty(elem->sessions))
		{
			NEURO_TRACE("Disconnecting a zombie active-parent connection's data", NULL);

			Neuro_SCleanEBuf(client_list, elem);

			return 1;
		}

		total = Neuro_GiveEBufCount(elem->sessions) + 1;

		while (total-- > 0)
		{
			buf = Neuro_GiveEBuf(elem->sessions, total);

			if (NNet_ClientExist2(network, buf->session) == 0)
			{
				NEURO_TRACE("Disconnecting a zombie active-layer connection's data", NULL);

				/* in case of an active client's layer, we need to remove the session 
				 * element from the username only and not the whole element.
				 */


				Neuro_SCleanEBuf(elem->sessions, buf);

				return 1;
			}
		}
	}
	else
	{
		if (NNet_ClientExist2(network, elem->client) == 0)
		{
			NEURO_TRACE("Disconnecting a zombie passive connection's data", NULL);

			/* in case of an active client's layer, we need to remove the session 
			 * element from the username only and not the whole element.
			 */


			Neuro_SCleanEBuf(client_list, elem);

			return 1;
		}
	}
	return 0;
}

/* outputs a pointer to a CList element that corresponds to an username. */
static CList*
clist_getD_from_name(char *name)
{
	u32 total = 0;
	CList *buf;

	if (Neuro_EBufIsEmpty(client_list))
		return NULL;

	total = Neuro_GiveEBufCount(client_list) + 1;

        while (total-- > 0)
        {
		buf = Neuro_GiveEBuf(client_list, total);

		if (clist_check_zombie(buf))
			continue;

		if (!strcmp(buf->name, name))
		{
			return buf;
		}
        }

	return NULL;
}

static void
clean_clist(void *src)
{
	CList *tmp;

	tmp = (CList*)src;

	if (tmp)
	{
		if (tmp->sessions)
			Neuro_CleanEBuf(&tmp->sessions);
	}
}

static void
Handle_Session(CList *lst, CONNECT_DATA *conn, char *data, u32 len)
{
	u32 total = 0;

	if (Neuro_EBufIsEmpty(lst->sessions))
	{
		return;
	}


}

/*-------------------- Global Functions ----------------------------*/

/*-------------------- Poll ------------------------*/

void
Server_Poll()
{
	u32 total = 0;
	CList *buf;

	if (Neuro_EBufIsEmpty(client_list))
		return;
}
	total = Neuro_GiveEBufCount(client_list) + 1;

        while (total-- > 0)
        {
		buf = Neuro_GiveEBuf(client_list, total);

		if (clist_check_zombie(buf))
			continue;
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
		case NET_QLIST:
		{
			u32 total = 0;
			CList *buf;

			/* info on all the active clients and their layers.
			 *
			 * see Pkt_List in global.h
			 */

			if (Neuro_EBufIsEmpty(client_list))
			{
				Packet_Reset(pktbuf);
				Packet_Push32(pktbuf, NET_DISCONNECT);
				NNet_Send(conn, Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));

				return 0;
			}

			total = Neuro_GiveEBufCount(client_list) + 1;

			while (total-- > 0)
			{
				buf = Neuro_GiveEBuf(client_list, total);

				if (clist_check_zombie(buf))
					continue;
				
				if (buf->client_type == 1)
				{
					u32 amount = 0;

					Packet_Reset(pktbuf);
					
					Packet_Push32(pktbuf, NET_LIST);
					
					Packet_PushString(pktbuf, 32, buf->name);
					
					if (!Neuro_EBufIsEmpty(buf->sessions))
						amount = Neuro_GiveEBufCount(buf->sessions) + 1;

					Packet_Push32(pktbuf, amount);

					/* also push a struct with more in depth informations 
					 * about the layers.
					 */

					NNet_Send(conn, Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));
				}
			}

			Packet_Reset(pktbuf);
			Packet_Push32(pktbuf, NET_DISCONNECT);
			NNet_Send(conn, Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));
		}
		break;

		case NET_CONNECT:
		{
			Pkt_Connect *connect;
			Session *session;
			CList *buf = NULL;

			connect = buffer;

			if (connect->client_type > 1)
			{
				printf("client has an unknown client type, dropping client\n");
				return 1;
			}

			if (connect->client_type == 1)
			{
				/* an active client */

				if (server_password)
				{
					if (!strcmp(server_password, connect->password))
					{
						printf("client GRANTED ");
						printf("active access to broadcasting server\n");
					}
					else
					{
						printf("client DENIED ");
						printf("active access to broadcasting server\n");
						return 1;
					}
				}

#if hardcode
				/* FIXME hardcode */
				if (hc_aclient)
				{
					printf("hardcoded hack code -- access denied\n");
					return 1;
				}

				hc_aclient = conn;
				/* FIXME end hardcode */
#endif /* hardcode */

				if (connect->name)
				{
					buf = clist_getD_from_name(connect->name);
				}

				if (!buf)
				{
					Neuro_AllocEBuf(client_list, sizeof(CList*), sizeof(CList));

					buf = Neuro_GiveCurEBuf(client_list);

					/* this will probably be removed eventually -- at least for 
					 * active clients.
					 */
					buf->client = conn;

					buf->client_type = connect->client_type;

				}

				if (Neuro_EBufIsEmpty(buf->sessions))
				{
					Neuro_CreateEBuf(&buf->sessions);
				}

				Neuro_AllocEBuf(buf->sessions, sizeof(Session*), sizeof(Session));

				session = Neuro_GiveCurEBuf(buf->sessions);

				session->session = conn;

				if (connect->name)
				{
					strncpy(buf->name, connect->name, 32);
					printf("Connection from client %s type %d\n", buf->name, buf->client_type);
				}

			}
			else
			{
				int _err = 0;
				CList *buf = NULL;

				/* a passive client */
				if (connect->name)
				{
					buf = clist_getD_from_name(connect->name);
				}
				else
					_err += 1;

				if (buf)
				{
					if (connect->layer > 0)
					{
						Session *sess;
						Listener *bufa;

						if (Neuro_EBufIsEmpty(buf->sessions))
						{
							sess = Neuro_GiveEBuf(buf->sessions, connect->layer - 1);

							if (sess)
							{
								Neuro_AllocEBuf(sess->listeners, sizeof(Listener*), sizeof(Listener));

								bufa = Neuro_GiveCurEBuf(sess->listeners);

								bufa->client = conn;
							}
							else
								_err += 1;
						}
						else
							_err += 1;

					}
					else
						_err += 1;
				}





				if (_err >= 1)
				{
					/* we disconnect the client, an error happened */

					/* a less drastic method might be better though ;) */
					return 1;
				}
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
			u32 total = 0;
			CList *buf;
			
			
			if (Neuro_EBufIsEmpty(client_list))
				return 0;

			total = Neuro_GiveEBufCount(client_list) + 1;

			while (total-- > 0)
			{
				buf = Neuro_GiveEBuf(client_list, total);

				if (clist_check_zombie(buf))
					continue;
				
				Handle_Session(buf, conn, data, len);
			}



#if hardcode
			/* printf("%c%c", bufa[0], bufa[1]); */
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

					if (clist_check_zombie(buf))
						continue;
					
					if (!buf->client_type)
					{
						if (len >= 512)
							printf("-- Packet size too big %d\n", len);
						/* printf("forwarding data len %d\n", len); */
						NNet_Send(buf->client, data, len);
					}
				}
			}
			/* FIXME end hardcode */
#endif /* hardcode */
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
	Neuro_SetcallbEBuf(client_list, clean_clist);

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

