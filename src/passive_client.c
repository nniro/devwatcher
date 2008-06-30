/* passive_client.c
 * Module : Passive_
 *
 */

/*-------------------- Extern Headers Including --------------------*/

#include <neuro/NEURO.h>
#include <string.h> /* memset strncpy */

#include <unistd.h> /* write */

#include <stdio.h> /* printf fprintf */

/*-------------------- Local Headers Including ---------------------*/

#include "core.h"
#include "packet.h"
#include "client.h"

/*-------------------- Main Module Header --------------------------*/

#include "passive_client.h"

/*--------------------      Other       ----------------------------*/

NEURO_MODULE_CHANNEL("passive_client");

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

static Packet *pktbuf;

static FILE *log;

/*-------------------- Static Prototypes ---------------------------*/

/*-------------------- Static Functions ----------------------------*/

static void
SendConnect(char *username, u32 layer, int client_type)
{
	Pkt_Connect connect;

	memset(&connect, 0, sizeof(Pkt_Connect));


	if (username)
		strncpy(connect.name, username, 32);

	connect.layer = layer;

	connect.client_type = client_type;

	Packet_Reset(pktbuf);

	Packet_Push32(pktbuf, NET_CONNECT);
	Packet_PushStruct(pktbuf, sizeof(Pkt_Connect), &connect);

	Client_SendPacket(Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));
}

/*-------------------- Global Functions ----------------------------*/

void
Passive_HandleData(const char *data, u32 len)
{
	/* write an output to a file called log_file2 to see the packets that the passive client recieves. */
	fprintf(log, "got [len %d]: \"", len);
	fwrite(data, len, 1, log);
	fprintf(log, "\"\n");
	fflush(log);

	/* printf("recieved len %d, data len %d\n", len, *length); */

	write(1, data, len);
}

/*-------------------- Poll -------------------------*/

void
Passive_Poll()
{
}

/*-------------------- Constructor Destructor ----------------------*/

int
Passive_Init(char *username, int layer)
{
	pktbuf = Packet_Create();

	NEURO_TRACE("Passive Client Init", NULL);

	{
		log = fopen("log_file2", "w");
	
		if (username && layer > 0)
		{
			SendConnect(username, layer, 0);
			return 0;
		}

		/* we send a packet to get
		 * the list of active sessions 
		 * currently on the server. 
		 */
		Packet_Push32(pktbuf, NET_QLIST);

		NEURO_TRACE("sending packet %s", Packet_GetBuffer(pktbuf));

		Client_SendPacket(Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));

		/* Client_SendPacket(Neuro_s("%d", NET_QLIST), 4); */



		return 0;
	}

	return 0;
}

void
Passive_Clean()
{
	Packet_Destroy(pktbuf);

	fclose(log);
}
