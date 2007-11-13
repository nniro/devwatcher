/* client.c
 * Module : Client_
 */

/*-------------------- Extern Headers Including --------------------*/

#include <neuro/NEURO.h>
#include <neuro/nnet/network.h>
#include <string.h> /* strlen() */

#include <stdio.h> /* printf() */
#include <unistd.h> /* execl() */

#include <sys/stat.h> /* mkfifo() */
#include <pty.h> /* openpty()  (in termios.h) tcgetattr() */
/*-------------------- Local Headers Including ---------------------*/

#include "core.h"
#include "packet.h"

/*-------------------- Main Module Header --------------------------*/

#include "client.h"

/*--------------------      Other       ----------------------------*/

NEURO_MODULE_CHANNEL("client");

typedef struct PTY_DATA
{
	int master;
	int slave;
	struct termios tt;
	struct winsize wsize;
}PTY_DATA;

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

static LISTEN_DATA *network;
static CONNECT_DATA *client;

static Packet *pktbuf;


static PTY_DATA *mpty;

/*-------------------- Static Prototypes ---------------------------*/

/*-------------------- Static Functions ----------------------------*/

/* simulates the new shell fork 
 * it sends the data to the screen and
 * to the server.
 */
static void
dooutput()
{
	register int c;
	char buf[BUFSIZ];
	FILE *fname;

	/* we close stdin */
	close(0); 
	/* we close slave */
	close(mpty->slave);

	fname = fopen("houba", "a");

	while (1 != 2)
	{
		c = read(mpty->master, buf, sizeof(buf));

		if (c <= 0)
			break;

		printf("herein\n");
		write(1, buf, c);
		fwrite(buf, 1, c, fname);
	}

	fclose(fname);
}

static PTY_DATA *
dopty()
{
	PTY_DATA *output;

	output = calloc(1, sizeof(PTY_DATA));

	/* we fetch the termios context for the current pty */
	tcgetattr(0, &output->tt);
	/* we fetch the window size for the current pty */
	ioctl(0, TIOCGWINSZ, (char*)&output->wsize);

	if (openpty(&output->master, &output->slave, NULL, &output->tt, &output->wsize) < 0)
	{
		NEURO_ERROR("Unable to create a new PTY", NULL);
		free(output);
		return NULL;
	}

	return output;
}

static int
dofork()
{
	int child = 0;

	child = fork();

	if (child < 0)
	{
		NEURO_ERROR("Creation of fork failed", NULL);
		return -1;
	}

	return child;
}

static int
doshell()
{
	/* starts a new session */
	setsid();
	/* this sets the current controlling  */
	ioctl(mpty->slave, TIOCSCTTY, 0);
	
	/* we close those because we want to simulate our own.
	 * We will then pipe those manually to both the current screen
	 * and to the server.
	 */
	close(mpty->master);
	dup2(mpty->slave, 0);
	dup2(mpty->slave, 1);
	dup2(mpty->slave, 2);
	close(mpty->slave);
	

	printf("entering a new shell, you can exit it at any time to stop this process\n");
	
	execl("/bin/bash", "-i", 0);

	return 0;
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
		int child;

		mpty = dopty();

		if (!mpty)
		{
			NEURO_ERROR("Pty creation failed", NULL);
			return 1;
		}

		/* we create 2 forks, one acts as a new shell and the other 
		 * continues the process of this code.
		 *
		 * child 0 continues to run this code
		 * and child > 0 runs the new shell
		 */

		child = dofork();

		if (child < 0)
			return 1;
		if (child > 0)
		{
			int subchild;

			subchild = child = fork();

			if (child == 0)
			{
				if (doshell())
					return 1;
			}
			else
			{
				dooutput();
				close(mpty->master);
			}

		}
	}

	/* SendConnect(client, username); */

	return 0;
}

void
Client_Clean()
{
	Packet_Destroy(pktbuf);

	if (mpty)
	{
		close(mpty->slave);
		free(mpty);
	}

        NNet_Destroy(network);	
}
