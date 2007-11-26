/* client.c
 * Module : Client_
 *
 * see the manuals : termios(3)
 */

/*-------------------- Extern Headers Including --------------------*/

#include <neuro/NEURO.h>
#include <neuro/nnet/network.h>
#include <string.h> /* strlen() */

#include <stdio.h> /* printf() */
#include <unistd.h> /* execl() */

#include <sys/stat.h> /* mkfifo() */
#define __USE_BSD 1 /* for having cfmakeraw() */
#include <termios.h> /* tcgetattr() tcsetattr() cfmakeraw() */
#include <pty.h> /* openpty()  (in termios.h) tcgetattr() */

#define __USE_POSIX 1 /* for having kill() */
#include <signal.h> /* signal() kill() */

#include <fcntl.h> /* fcntl() */

#include <sys/time.h>
#include <sys/wait.h> /* wait3() */
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

/* use forks instead of spoons */
#define USE_FORKS 1

#define ALIVE_TIME 300

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

static LISTEN_DATA *network;
static CONNECT_DATA *client;

static Packet *pktbuf;

static int client_t;

static PTY_DATA *mpty;

static int child;
static int subchild;

/* time left until we send an alive packet type */
static t_tick alive_time; 

/* transfer fifo */
static int xfer_fifo[2];

/*-------------------- Static Prototypes ---------------------------*/

/*-------------------- Static Functions ----------------------------*/

static void
done()
{
	if (subchild)
	{
		close(mpty->master);
	}
	else
	{
		tcsetattr(0, TCSAFLUSH, &mpty->tt);
		printf("program execution finished PID %d\n", getpid());
	}

	exit(0);
}

static void
finish(int dummy)
{
	int status;
	register int pid;
	register int die = 0;

	NEURO_TRACE("CATCHED SIGNAL SIGCHLD", NULL);

	while ((pid = wait3(&status, WNOHANG, 0)) > 0)
	{
		if (pid == child)
			die = 1;
	}

	printf("pid %d ppid %d child %d exit status %d\n", getpid(), getppid(), child, status);
	NEURO_TRACE("SIGCHLD %d", pid);
	if (die)
		done();
}

static void
resize(int dummy)
{
	ioctl(0, TIOCGWINSZ, (char*)&mpty->wsize);
	ioctl(mpty->slave, TIOCGWINSZ, (char*)&mpty->wsize);

	kill(child, SIGWINCH);
}

/* we disable input characters to be drawn on the 
 * main tty. (we work on the pty, remember ;)).
 */
static void
fixtty()
{
	struct termios rtt;

	/* return; */

	rtt = mpty->tt;

	/* we assign default values to the termios structure */
	cfmakeraw(&rtt);

	/* we remove the flag that sets if input characters are drawn on the screen 
	 * from the control modes flag.
	 */
	/* rtt.c_lflag |= ECHO; */
	rtt.c_lflag &= ~ECHO;

	/* we set the termios struct to the tty */
	tcsetattr(0, TCSAFLUSH, &rtt);
}

static void
doinput()
{
	register int c;
	char buf[BUFSIZ];

	while((c = read(0, buf, BUFSIZ)) > 0)
		write(mpty->master, buf, c);

	printf("%s() DONE\n", __FUNCTION__);

	done();
}

static int
doinput2()
{
	register int c;
	char buf[BUFSIZ];

	c = read(0, buf, BUFSIZ);

	if (c < 0)
		return 1;
	
	/* NEURO_WARN("herein", NULL); */
	write(mpty->master, buf, c);

	return 0;
}

/* simulates the new shell fork 
 * it sends the data to the screen and
 * to the server.
 */
static void
dooutput()
{
	register int c;
	char buf[BUFSIZ];
	/*FILE *fname;*/

	/* we close stdin */
	close(0); 
	/* we close slave */
	close(mpty->slave);

	/* fname = fopen("houba", "w"); */

	close(xfer_fifo[0]);

	while (1 != 2)
	{
		c = read(mpty->master, buf, sizeof(buf));

		if (c <= 0)
			break;

		/* printf("herein\n"); */
		write(1, buf, c);

		write(xfer_fifo[1], buf, c);


		/* fwrite(buf, 1, c, fname); */
		/*fflush(fname);*/


	}

	close(xfer_fifo[1]);
	/* fclose(fname); */

	done();
}

/* simulates the new shell fork 
 * it sends the data to the screen and
 * to the server.
 */
static int
dooutput2()
{
	register int c;
	char buf[BUFSIZ];
	FILE *fname;

	/* we close stdin */
	close(0); 
	/* we close slave */
	close(mpty->slave);

	fname = fopen("houba", "w");

	c = read(mpty->master, buf, sizeof(buf));

	if (c <= 0)
		return 1;

	write(1, buf, c);

	return 0;
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
	
	execl("/bin/bash", "bash", "-i", 0);

	perror("/bin/bash");
	kill(0, SIGTERM);
	done();

	return 0;
}

static void
SendConnect(char *username, char *password, int client_type)
{
	Pkt_Connect connect;

	if (username)
		strncpy(connect.name, username, 32);

	if (password)
		strncpy(connect.password, password, 32);

	connect.client_type = client_type;

	Packet_Reset(pktbuf);

	Packet_Push32(pktbuf, NET_CONNECT);
	Packet_PushStruct(pktbuf, sizeof(Pkt_Connect), &connect);

	NNet_Send(client, Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));
}

static void
SendConnect2(char *username, u32 layer, int client_type)
{
	Pkt_Connect connect;

	if (username)
		strncpy(connect.name, username, 32);

	connect.layer = layer;

	connect.client_type = client_type;

	Packet_Reset(pktbuf);

	Packet_Push32(pktbuf, NET_CONNECT);
	Packet_PushStruct(pktbuf, sizeof(Pkt_Connect), &connect);

	NNet_Send(client, Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));
}

/*-------------------- Global Functions ----------------------------*/

/*-------------------- Poll -------------------------*/

static FILE *log;

void
Client_Poll()
{
	if (client_t)
	{
		if (USE_FORKS == 0)
		{
			dooutput2();

			doinput2();
		}
		else
		{
			char buf[BUFSIZ];
			char *buffer;
			register int c, i, t;

			while ((c = read(xfer_fifo[0], buf, sizeof(buf))) > 0)
			{
				buffer = buf;

				t = 0;
				i = c;
				while (c > 0)
				{
					i = c;
					if (i > 300)
						i = 300;

					/* write an output to a file called log_file to debug the packets sent to the server. */
					fprintf(log, "c %d i %d t %d --> buffer \"%s\"\n", c, i, t, buffer);
					fflush(log);

					t += i;

					c -= i;

					Packet_Reset(pktbuf);

					Packet_Push32(pktbuf, NET_DATA);
					Packet_Push32(pktbuf, i);
					Packet_PushString(pktbuf, i, buffer);

					if (Packet_GetLen(pktbuf) > (4 + 4 + i))
					{
						NEURO_ERROR("Packet bigger than it should by %d bytes", Packet_GetLen(pktbuf) - 200);
					}

					NNet_Send(client, Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));
					buffer = &buf[t];
				}

				/* temporarily... this is not necessary */
				memset(buf, 0, t);
			}
		}
	}

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
packet_handler(CONNECT_DATA *conn, char *data, u32 len)
{
	Pkt_Header *whole;
	void *buffer;

	whole = (Pkt_Header*)data;

	buffer = &whole[1];

	switch (whole->type)
	{
		case NET_DATA:
		{
			char *buf;
			int *length;

			length = buffer;
			buf = (char*)&length[1];

			/* write an output to a file called log_file2 to see the packets that the passive client recieves. */
			fprintf(log, "got [%d]: \"", *length);
			fwrite(buf, *length, 1, log);
			fprintf(log, "\"\n");
			fflush(log);

			/* printf("recieved len %d, data len %d\n", len, *length); */

			write(1, buf, *length);
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

	NNet_SetTimeout(client, 0);

	pktbuf = Packet_Create();

	if (client_type == 1)
	{

		log = fopen("log_file", "w");

		/* FIXME hardcoded */
		if (pipe(xfer_fifo))
		{
			NEURO_ERROR("Pipe creation failure", NULL);
			return 1;
		}
		/* FIXME end hardcoded */

		mpty = dopty();

		if (!mpty)
		{
			NEURO_ERROR("Pty creation failed", NULL);
			return 1;
		}

		fixtty();

		if (USE_FORKS)
		{

			signal(SIGCHLD, finish);

			/* we create 2 forks, one acts as a new shell and the other 
			 * continues the process of this code.
			 *
			 * child 0 continues to run this code
			 * and child > 0 runs the new shell
			 */

			child = fork();

			if (child < 0)
			{
				NEURO_ERROR("Creation of fork failed", NULL);
				return 1;
			}

			/* printf("CHILD %d\n", getpid()); */
			if (child == 0)
			{
				child = fork();

				/* printf("CHILD2 %d\n", getpid()); */
				if (child == 0)
				{
					subchild = child = fork();

					/* printf("CHILD3 %d\n", getpid()); */
					if (child == 0)
						doshell();
					else
						dooutput();
				}

				doinput();
			}
			else
				signal(SIGWINCH, resize);
		}
		else
		{
			signal(SIGWINCH, resize);

			child = fork();
			
			if (child < 0)
			{
				NEURO_ERROR("Creation of fork failed", NULL);
				return 1;
			}

			if (child > 0)
				doshell();
		}

		fcntl(xfer_fifo[0], F_SETFL, O_NONBLOCK);
		close(xfer_fifo[1]);
	}
	else
	{
		log = fopen("log_file2", "w");
	
		if (username && layer > 0)
		{
			SendConnect2(username, layer, client_type);
			return 0;
		}

		/* we send a packet to get
		 * the list of active sessions 
		 * currently on the server. 
		 */
		Packet_Push32(pktbuf, NET_QLIST);

		NNet_Send(client, Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));



		return 0;
	}

	client_t = client_type;

	SendConnect(username, password, client_type);

	return 0;
}

void
Client_Clean()
{
	Packet_Destroy(pktbuf);

	close(xfer_fifo[0]);

	if (mpty)
	{
		close(mpty->master);
		close(mpty->slave);
		tcsetattr(0, TCSAFLUSH, &mpty->tt);
		free(mpty);
	}

        NNet_Destroy(network);	
}
