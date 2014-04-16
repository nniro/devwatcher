/* server.c
 * Module : Server_
 *
 * the server module of the project
 */

/*-------------------- Extern Headers Including --------------------*/

#include <neuro/NEURO.h>
#include <neuro/nnet/network.h>
#include <string.h> /* strncmp */

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
typedef struct Session Session;

struct CList
{
	u8 client_type; /* 0 is passive, 1 is active */
	char name[32]; /* active clients only */

	Session *audit; /* for passive clients only, this is the pointer to the connected session. */

	/* sessions (layers of active clients that a passive client can watch) */
	/* active clients only */
	EBUF *sessions; /* contains Session type of elements */
};

struct Session
{
	/* each session need statistics like buffer size, how much bytes transfered 
	 * and how long since the last change. All this for the list query packet.
	 */
	int active; /* 0 inactive, 1 active */

	char *summary;
	char *description;

	int cols, rows; /* the size of the broadcasting screen */

	NNET_SLAVE *session;

	/* list of passive clients that listen that session */
	EBUF *listeners; /* contains Listener elements */
};

typedef struct Listener Listener;

struct Listener
{
	NNET_SLAVE *client;
};

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

static NNET_SLAVE *server;

static EBUF *client_list; /* CList */

static Packet *pktbuf;

/* main server password */
static char *server_password;

/*-------------------- Static Prototypes ---------------------------*/

static int packet_handler(NNET_SLAVE *conn, const char *data, u32 len);

/*-------------------- Static Functions ----------------------------*/

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
			NEURO_WARN("Disconnecting a passive connection", NULL);

			/*Packet_Reset(pktbuf);
			Packet_Push32(pktbuf, NET_DISCONNECT);
			NNet_Send(tmp->client, Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));
			*/

			/*NNet_DisconnectClient(tmp->client);*/
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

		if (len >= 512)
			NEURO_WARN("Broadcast Packet Bigger than 511 : %d", len);

		NNet_Send(buf->client, data, len);
	}
}

static int
Handle_Session(CList *lst, NNET_SLAVE *conn, const char *data, u32 len)
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

static CList *
lookupCList(const char *name)
{
	CList *buf = NULL;
	u32 total = 0;

	if (!name || Neuro_EBufIsEmpty(client_list))
		return NULL;

	total = Neuro_GiveEBufCount(client_list) + 1;

	while (total-- > 0)
	{
		buf = Neuro_GiveEBuf(client_list, total);

		if (!strncmp(name, buf->name, 32))
			return buf;
	}

	return NULL;
}

static Listener *
lookupListener(EBUF *listeners, const NNET_SLAVE *slave)
{
	Listener *buf = NULL;
	u32 total = 0;

	if (!slave || Neuro_EBufIsEmpty(listeners))
		return NULL;

	total = Neuro_GiveEBufCount(listeners) + 1;

	while (total-- > 0)
	{
		buf = Neuro_GiveEBuf(listeners, total);

		if (slave == buf->client)
			return buf;
	}

	return NULL;
}

static Session *
lookupSession(EBUF *sessions, const NNET_SLAVE *slave)
{
	Session *buf = NULL;
	u32 total = 0;

	if (!slave || Neuro_EBufIsEmpty(sessions))
		return NULL;

	total = Neuro_GiveEBufCount(sessions) + 1;

	while (total-- > 0)
	{
		buf = Neuro_GiveEBuf(sessions, total);

		if (slave == buf->session)
			return buf;
	}

	return NULL;
}

/*-------------------- Global Functions ----------------------------*/

/*-------------------- Poll ------------------------*/

int
Server_Poll(NNET_STATUS *status)
{
	u32 _err = 0;
	switch (NNet_GetStatus(status))
	{
		case State_NoData:
		{
			/* NEURO_TRACE("No data", NULL); */
		}
		break;

		case State_NewClient:
		{
			NEURO_TRACE("New client connection %s", NNet_GetIP(NNet_GetSlave(status)));
		}
		break;

		case State_Disconnect:
		{
			/* closing the server */
			return 1;
		}
		break;

		case State_ClientDisconnect:
		{
			CList *buf = NULL;


			buf = (CList *)NNet_GetData(NNet_GetSlave(status));
			if (buf != NULL)
			{

				NEURO_TRACE("%s client disconnected", buf->client_type == 1 ? "Active" : "Passive");

				if (buf->client_type == 0) /* passive connection */
				{
					Listener *tmp = lookupListener(buf->audit->listeners, NNet_GetSlave(status));
					Neuro_SCleanEBuf(buf->audit->listeners, tmp);
				}
				/* nothing special need to be done for active connections */

				Neuro_SCleanEBuf(client_list, buf);
			}
		}
		break;

		case State_DataAvail:
		{
			_err = packet_handler(NNet_GetSlave(status), NNet_GetPacket(status), NNet_GetPacketLen(status));
		}
		break;

		default:
		{
			NEURO_ERROR("unknown status %d", NNet_GetStatus(status));
			return 1;
		}
		break;
	}

	return _err;
}

/*-------------------- Packets handler Poll ------------------------*/

static int
packet_handler(NNET_SLAVE *conn, const char *data, u32 len)
{
        Pkt_Header *whole;
	void *buffer;

        whole = (Pkt_Header*)data;

	buffer = &whole[1];

	/* NEURO_TRACE("Packet recieved, len %d", len); */

	if (!whole && len > 0)
	{
		NEURO_ERROR("The data pointer is empty with len %d", len);
		return 0;
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
				NNet_DisconnectClient(conn);

				return 0;
			}

			total = Neuro_GiveEBufCount(client_list) + 1;

			while (total-- > 0)
			{
				buf = Neuro_GiveEBuf(client_list, total);
				
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
			NNet_DisconnectClient(conn);
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
				NNet_DisconnectClient(conn);
				return 0;
			}

			if (connect->client_type == 1) /* an active client */
			{
				if (server_password)
				{
					if (!strcmp(server_password, connect->password))
					{
						NEURO_TRACE("client GRANTED active access to broadcasting server", NULL);

						Packet_Reset(pktbuf);

						Packet_Push32(pktbuf, NET_INFO);

						Packet_Push32(pktbuf, 1); /* access granted */

						/* we don't send screen size infos */
						Packet_Push32(pktbuf, 0);
						Packet_Push32(pktbuf, 0);

						NNet_Send(conn, Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));
					}
					else
					{
						NEURO_TRACE("client DENIED active access to broadcasting server", NULL);

						Packet_Reset(pktbuf);

						Packet_Push32(pktbuf, NET_INFO);

						Packet_Push32(pktbuf, 0); /* access denied */

						/* we don't send screen size infos */
						Packet_Push32(pktbuf, 0);
						Packet_Push32(pktbuf, 0);

						NNet_Send(conn, Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));
						NNet_DisconnectClient(conn);
						return 0;
					}
				}

				if (connect->name)
				{
					buf = NNet_GetData(conn);
				}

				if (!buf)
				{
					Neuro_AllocEBuf(client_list, sizeof(CList*), sizeof(CList));

					buf = Neuro_GiveCurEBuf(client_list);

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

				NNet_SetData(conn, buf);
			}
			else if (connect->client_type == 0) /* a passive client */
			{
				int _err = 0;
				CList *buf = NULL;

				if (connect->name)
				{
					/*buf = NNet_GetData(conn);*/
					buf = lookupCList(connect->name);
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

							if (sess && sess->active == 1)
							{
								CList *current;

								Neuro_AllocEBuf(client_list, sizeof(CList*), sizeof(CList));
								current = Neuro_GiveCurEBuf(client_list);
								current->client_type = connect->client_type;
								current->audit = sess;


								Neuro_AllocEBuf(sess->listeners, sizeof(Listener*), sizeof(Listener));

								bufa = Neuro_GiveCurEBuf(sess->listeners);

								bufa->client = conn;

								NEURO_TRACE("New passive connection", NULL);
								/* temporary debugging output */
								fprintf(stderr, "User %s Layer %d\n", buf->name, connect->layer);

								Packet_Reset(pktbuf);
								Packet_Push32(pktbuf, NET_INFO);
								Packet_Push32(pktbuf, 1); /* access granted */
								/* we send screen size infos */
								Packet_Push32(pktbuf, sess->cols);
								Packet_Push32(pktbuf, sess->rows);

								NNet_Send(conn, Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));

								NNet_SetData(conn, current);
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

					Packet_Reset(pktbuf);

					Packet_Push32(pktbuf, NET_INFO);

					Packet_Push32(pktbuf, 0); /* access denied */

					/* we don't send screen size infos */
					Packet_Push32(pktbuf, 0);
					Packet_Push32(pktbuf, 0);

					NNet_Send(conn, Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));

					NNet_DisconnectClient(conn);
					return 0;
				}
			}
			else
			{
				NEURO_WARN("Client sent an invalid client type, disconnecting", NULL);
				NNet_DisconnectClient(conn);
				return 0;
			}
		}
		break;

		case NET_WSIZE:
		{
			CList *buf;
			Session *session;

			buf = NNet_GetData(conn);

			if (!buf)
			{
				NEURO_WARN("Invalid Client detected, disconnecting", NULL);
				NNet_DisconnectClient(conn);
				return 0;
			}

			session = lookupSession(buf->sessions, conn);

			if (!session)
			{
				NEURO_WARN("Active client attempts to set WSize without a session", NULL);
				return 0;
			}

			if (len >= sizeof(Pkt_WSize))
			{
				Pkt_WSize *tmp;

				tmp = (Pkt_WSize*)buffer;

				session->cols = tmp->cols;
				session->rows = tmp->rows;

				if (session->cols > 0 && session->rows > 0)
				{
					NEURO_TRACE("Active connection screen size : %s", 
						Neuro_s("%dx%d", session->cols, session->rows));
					session->active = 1;
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
			Session *session;

			buf = NNet_GetData(conn);

			if (!buf)
			{
				NEURO_WARN("Invalid Client detected, disconnecting", NULL);
				NNet_DisconnectClient(conn);
				return 0;
			}
			
			session = lookupSession(buf->sessions, conn);

			if (!session)
			{
				NEURO_WARN("Active client attempts to broadcast without a session", NULL);
				return 0;
			}

			if (session->cols == 0 || session->rows == 0)
			{
				NEURO_WARN("Active client attempts to broadcast without having sent the correct screen size information", NULL);
				NNet_DisconnectClient(conn);
				return 0;
			}

			if (session->active == 0)
			{
				NEURO_WARN("Active client sent data to an inactive session", NULL);
				return 0;
			}
			
			if (Neuro_EBufIsEmpty(client_list))
				return 0;

			total = Neuro_GiveEBufCount(client_list) + 1;

			while (total-- > 0)
			{
				buf = Neuro_GiveEBuf(client_list, total);
				
				if (Handle_Session(buf, conn, data, len))
					break;
			}
		}
		break;

                default:
                {
                        NEURO_WARN("%s", Neuro_s("Unhandled packet type recieved len %d data [%x %x %x %x]", len, data[0], data[1], data[2], data[3]));
			NEURO_TRACE("Disconnecting client", NULL);
			NNet_DisconnectClient(conn);
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
Server_Init(NNET_MASTER *master, char *password, int port)
{
	/* NNet_ServerTogglePacketSize(network); */

	server = NNet_Listen(master, "0.0.0.0", port);

	if(!server)
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
}

