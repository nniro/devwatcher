/* server.c
 * Module : Server_
 *
 * the server module of the project
 */

/*-------------------- Extern Headers Including --------------------*/

#include <neuro/NEURO.h>
#include <neuro/nnet/network.h>
#include <string.h>

#include <signal.h> /* signal() */

#include <stdio.h> /* stderr printf() fprintf() */

/*-------------------- Local Headers Including ---------------------*/

#include "core.h"
#include "packet.h"

#include "main.h" /* Main_Exit() */

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
clean_session(void *src)
{
	Session *tmp;

	tmp = (Session*)src;

	if (tmp)
	{
		Neuro_CleanEBuf(&tmp->listeners);
	}
}

static void
clean_clist(void *src)
{
	CList *tmp;

	tmp = (CList*)src;

	if (tmp)
	{
		Neuro_CleanEBuf(&tmp->sessions);
	}
}

static void
clean_listener(void *src)
{
	Listener *tmp;

	tmp = (Listener*)src;

	if (tmp)
	{
		if (tmp->client)
		{
			NEURO_WARN("Disconnecting a residual passive connection", NULL);

			/*Packet_Reset(pktbuf);
			Packet_Push32(pktbuf, NET_DISCONNECT);
			NNet_Send(tmp->client, Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));
			*/

			NNet_DisconnectClient(network, tmp->client);
		}
	}
}

static void
Broadcast_data(EBUF *listen, const char *data, u32 len)
{
	u32 total = 0;
	Listener *buf = NULL;

	if (Neuro_EBufIsEmpty(listen))
	{
		return;
	}

	total = Neuro_GiveEBufCount(listen) + 1;

	while (total-- > 0)
	{
		buf = Neuro_GiveEBuf(listen, total);

		if (NNet_ClientExist2(network, buf->client) == 0)
		{
			NEURO_TRACE("Disconnecting a zombie passive connection's data", NULL);

			/* in case of an active client's layer, we need to remove the session 
			 * element from the username only and not the whole element.
			 */

			Neuro_SCleanEBuf(listen, buf);

			continue;
		}

		if (len >= 512)
			NEURO_WARN("Broadcast Packet Bigger than 511 : %d", len);

		NNet_Send(buf->client, data, len);
	}
}

static int
Handle_Session(CList *lst, CONNECT_DATA *conn, const char *data, u32 len)
{
	u32 total = 0;
	Session *buf;

	if (Neuro_EBufIsEmpty(lst->sessions))
	{
		return 0;
	}

	total = Neuro_GiveEBufCount(lst->sessions) + 1;

	while (total-- > 0)
	{
		buf = Neuro_GiveEBuf(lst->sessions, total);

		if (buf->session == conn)
		{
			Broadcast_data(buf->listeners, data, len);
			return 1;
		}
	}

	return 0;
}

/*-------------------- Global Functions ----------------------------*/

/*-------------------- Poll ------------------------*/

void
Server_Poll()
{
	u32 total = 0;
	CList *buf;

	if (Neuro_EBufIsEmpty(client_list))
	{
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
packet_handler(CONNECT_DATA *conn, const char *data, u32 len)
{
        Pkt_Header *whole;
	void *buffer;

        whole = (Pkt_Header*)data;

	buffer = &whole[1];

	NEURO_TRACE("Packet recieved, len %d", len);

	if (!whole && len > 0)
	{
		NEURO_ERROR("The data pointer is empty with len %d", len);
		return 0;
	}
	else
	{
		if (!whole && len == 0)
		{
			NEURO_TRACE("New client connection", NULL);
			return 0;
		}
	}

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

			NEURO_TRACE("NET_QLIST processing", NULL);

			if (Neuro_EBufIsEmpty(client_list))
			{
				NEURO_TRACE("active client_list is empty, disconnecting client", NULL);
				Packet_Reset(pktbuf);
				Packet_Push32(pktbuf, NET_DISCONNECT);
				NNet_Send(conn, Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));

				return 1;
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

					NEURO_TRACE("Pushing active client list to client", NULL);
					/* also push a struct with more in depth informations 
					 * about the layers.
					 */

					NNet_Send(conn, Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));
				}
			}

			NEURO_TRACE("disconnecting client", NULL);

			Packet_Reset(pktbuf);
			Packet_Push32(pktbuf, NET_DISCONNECT);
			NNet_Send(conn, Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));
			return 0;
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
				NEURO_WARN("client has an unknown client type, dropping client", NULL);
				return 1;
			}

			if (connect->client_type == 1)
			{
				/* an active client */

				if (server_password)
				{
					if (!strcmp(server_password, connect->password))
					{
						NEURO_WARN("client GRANTED active access to broadcasting server", NULL);
					}
					else
					{
						NEURO_WARN("client DENIED active access to broadcasting server", NULL);
						return 1;
					}
				}

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
					Neuro_SetcallbEBuf(buf->sessions, clean_session);
				}

				Neuro_AllocEBuf(buf->sessions, sizeof(Session*), sizeof(Session));

				session = Neuro_GiveCurEBuf(buf->sessions);

				session->session = conn;

				Neuro_CreateEBuf(&session->listeners);
				Neuro_SetcallbEBuf(session->listeners, clean_listener);

				if (connect->name)
				{
					strncpy(buf->name, connect->name, 32);
					/* printf("Connection from client %s type %d\n", buf->name, buf->client_type); */
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
				{
					NEURO_TRACE("Couldn't find corresponding active name", NULL);
					_err += 1;
				}

				if (buf)
				{
					NEURO_TRACE("Passive client wants layer %d", connect->layer);

					if (connect->layer > 0)
					{
						Session *sess;
						Listener *bufa;

						if (!Neuro_EBufIsEmpty(buf->sessions))
						{
							sess = Neuro_GiveEBuf(buf->sessions, connect->layer - 1);

							if (sess)
							{
								Neuro_AllocEBuf(sess->listeners, sizeof(Listener*), sizeof(Listener));

								bufa = Neuro_GiveCurEBuf(sess->listeners);

								bufa->client = conn;

								NEURO_TRACE("New passive connection", NULL);
								/* temporary debugging output */
								fprintf(stderr, "User %s Layer %d\n", buf->name, connect->layer);
							}
							else
								_err += 1;
						}
						else
						{
							NEURO_TRACE("Couldn't find any sessions", NULL);
							_err += 1;
						}

					}
					else
					{
						NEURO_TRACE("Layer has to be > than 0", NULL);
						_err += 1;
					}
				}
				else
				{
					NEURO_TRACE("No active connections available", NULL);
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
				
				if (Handle_Session(buf, conn, data, len))
					break;
			}
		}
		break;

                default:
                {
                        NEURO_WARN("%s", Neuro_s("Unhandled packet type recieved len %d data [%x %x %x %x]", len, data[0], data[1], data[2], data[3]));
                }
                break;
	}

	Packet_Reset(pktbuf);

	return 0;
}

/*-------------------- Constructor Destructor ----------------------*/

static void
clean_program(int dummy)
{
	Main_Exit();
}

int
Server_Init(char *password, int port)
{
	network = NNet_Create(packet_handler, 0);

	NNet_ServerTogglePacketSize(network);

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

	signal(SIGINT, clean_program);

	return 0;
}

void
Server_Clean()
{
	Packet_Destroy(pktbuf);

	Neuro_CleanEBuf(&client_list);
	NNet_Destroy(network);
}

