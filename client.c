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

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

static LISTEN_DATA *network;
static CONNECT_DATA *client;

static Packet *pktbuf;


static PTY_DATA *mpty;

static int child;
static int subchild;

/*-------------------- Static Prototypes ---------------------------*/

/*-------------------- Static Functions ----------------------------*/

static void
done()
{
	NEURO_TRACE("Child finished", NULL);
	if (subchild)
	{
		close(mpty->master);
	}
	else
	{
		tcsetattr(0, TCSAFLUSH, &mpty->tt);
		printf("program execution finished\n");
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

	printf("pid %d ppid %d child %d\n", getpid(), getppid(), child);
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
	{
		/* NEURO_WARN("herein", NULL); */
		write(mpty->master, buf, c);
	}

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
	FILE *fname;

	/* we close stdin */
	close(0); 
	/* we close slave */
	close(mpty->slave);

	fname = fopen("houba", "w");

	while (1 != 2)
	{
		c = read(mpty->master, buf, sizeof(buf));

		if (c <= 0)
			break;

		/* printf("herein\n"); */
		write(1, buf, c);
		/*fwrite(buf, 1, c, fname);

		fflush(fname);*/
	}

	fclose(fname);

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
	
	execl("/bin/bash", "bash", "-i", 0);

	perror("/bin/bash");
	kill(0, SIGTERM);
	done();

	return 0;
}

/*-------------------- Global Functions ----------------------------*/

/*-------------------- Poll -------------------------*/

void
Client_Poll()
{
	if (USE_FORKS == 0)
	{
		dooutput2();

		doinput2();
	}
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

			child = dofork();

			if (child < 0)
			{
				NEURO_ERROR("Creation of fork failed", NULL);
				return 1;
			}

			NEURO_TRACE("CHILD %d", child);
			if (child > 0)
			{
				child = fork();

				NEURO_TRACE("CHILD2 %d", child);
				if (child == 0)
				{
					subchild = child = fork();

					NEURO_TRACE("CHILD3 %d", child);
					if (child == 0)
						doshell();
					else
						dooutput();
				}
				else
					signal(SIGWINCH, resize);

				doinput();
			}
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
		close(mpty->master);
		close(mpty->slave);
		tcsetattr(0, TCSAFLUSH, &mpty->tt);
		free(mpty);
	}

        NNet_Destroy(network);	
}
