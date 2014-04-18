/* active_client.c
 * Module : Active_
 *
 * see the manuals : termios(3)
 */

/*-------------------- Extern Headers Including --------------------*/

#include <neuro/NEURO.h>
#include <neuro/packet.h>

#include <string.h> /* strlen memset strncpy */

#include <stdio.h> /* printf fprintf */
#include <unistd.h> /* execl */

#include <sys/ioctl.h> /* ioctl */
#include <sys/stat.h> /* mkfifo */
#define __USE_BSD 1 /* for having cfmakeraw */
#include <termios.h> /* tcgetattr tcsetattr cfmakeraw */
#include <pty.h> /* openpty (in termios.h) tcgetattr */

#define __USE_POSIX 1 /* for having kill */
#include <signal.h> /* signal kill */

#include <fcntl.h> /* fcntl */

#include <sys/time.h>
#include <sys/wait.h> /* wait3 */

#include <errno.h> /* errno */
/*-------------------- Local Headers Including ---------------------*/

#include "main.h"
#include "core.h"

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

static Packet *pktbuf;

static PTY_DATA *mpty;

static int child;
static int subchild;

/* transfer fifo */
static int xfer_fifo[2];

static FILE *log;

static int activated = 0; /* mean to activate or deactivate the poll */

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
		printf(Neuro_s("program execution finished PID %d\r\n", getpid()));
	}
	Main_Exit();
	/*exit(0);*/
}

static void
finish(int dummy)
{
	int status;
	register int pid;
	register int die = 0;

	TRACE(Neuro_s("Pid %d -- CATCHED SIGNAL SIGCHLD", getpid()));

	while ((pid = wait3(&status, WNOHANG, 0)) > 0)
	{
		if (pid == child)
			die = 1;
	}

	printf(Neuro_s("pid %d ppid %d child %d exit status %d\r\n", getpid(), getppid(), child, status));
	TRACE(Neuro_s("Pid %d -- SIGCHLD %d", getpid(), pid));
	if (die)
	{
		done();
		/* exit(0); */
	}
}

static void
resize(int dummy)
{
	ioctl(0, TIOCGWINSZ, (char*)&mpty->wsize);
	ioctl(mpty->slave, TIOCSWINSZ, (char*)&mpty->wsize);

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

	printf(Neuro_s("doinput pid %d\r\n", getpid()));

	while((c = read(0, buf, BUFSIZ)) > 0)
		write(mpty->master, buf, c);

	printf(Neuro_s("%s() DONE\r\n", __FUNCTION__));

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

	printf(Neuro_s("dooutput pid %d\r\n", getpid()));

	/* we close stdin */
	close(0); 
	/* we close slave */
	close(mpty->slave);

	/* fname = fopen("houba", "w"); */

	close(xfer_fifo[0]);

	while (1 != 2)
	{
		c = read(mpty->master, buf, BUFSIZ);

		if (c <= 0)
			break;

		/*
		if (c == BUFSIZ)
			printf("FULL BUFFER!\n");
		else
			printf("BUFFER IS USED AT %d%%\n", (int)(((double)c / (double)BUFSIZ) * (double)100));
		*/

		/* printf("herein\n"); */
		/* write(stdout, buf, c); */
		fwrite(buf, 1, c, stdout);
		fflush(stdout);

		write(xfer_fifo[1], buf, c);


		/* fwrite(buf, 1, c, fname); */
		/*fflush(fname);*/


	}

	/* we send the EOT(end of transmission) packet (4 consecutive \0 bytes) */
	buf[0] = 4;
	write(xfer_fifo[1], buf, 1);
	write(xfer_fifo[1], buf, 1);
	write(xfer_fifo[1], buf, 1);
	write(xfer_fifo[1], buf, 1);

	close(xfer_fifo[1]);
	/* fclose(fname); */

	printf("dooutput() DONE\r\n");

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
		ERROR("Unable to create a new PTY");
		free(output);
		return NULL;
	}

	TRACE(Neuro_s("Created the PTY master %d slave %d", output->master, output->slave));

	return output;
}

static int
doshell()
{

	printf(Neuro_s("doshell pid %d\r\n", getpid()));

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
	
	{
		char *welcome = "entering a new shell, you can exit it at any time to stop this process";
		printf(Neuro_s("%s -- size %d\n", welcome, strlen(welcome)));
	}
	
	execl("/bin/bash", "bash", "-i", 0);

	/* Normally, this part, after execl is never ran.
	 * It may only be ran if there is an error in execl.
	 */ 

	perror("/bin/bash");
	
	kill(0, SIGTERM);

	printf("Leaving the bash instance\n");

	done();

	return 0;
}

static void
SendConnect(char *username, char *password, int client_type)
{
	Pkt_Connect connect;

	memset(&connect, 0, sizeof(Pkt_Connect));

	if (username)
		strncpy(connect.name, username, 32);

	if (password)
		strncpy(connect.password, password, 32);

	connect.client_type = client_type;

	Packet_Reset(pktbuf);

	Packet_Push32(pktbuf, NET_CONNECT);
	Packet_Push32(pktbuf, 0);
	Packet_PushStruct(pktbuf, sizeof(Pkt_Connect), &connect);

	Client_SendPacket(Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));
}

/*-------------------- Global Functions ----------------------------*/

int
Active_StartSession()
{	
	log = fopen("log_file", "w");

	/* FIXME hardcoded */
	if (pipe(xfer_fifo))
	{
		ERROR("Pipe creation failure");
		return 1;
	}
	/* FIXME end hardcoded */

	mpty = dopty();

	if (!mpty)
	{
		ERROR("Pty creation failed");
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
			ERROR("Creation of fork failed");
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
				TRACE(Neuro_s("Process %d -- PTY size : row %d col %d", 
					getpid(),
					mpty->wsize.ws_row, 
					mpty->wsize.ws_col));
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
			ERROR("Creation of fork failed");
			return 1;
		}

		if (child > 0)
			doshell();
	}

	fcntl(xfer_fifo[0], F_SETFL, O_NONBLOCK);
	close(xfer_fifo[1]);

	/* activate the polling */
	activated = 1;

	return 0;
}

void
Active_SendWSize()
{
	struct winsize wsize;

	ioctl(0, TIOCGWINSZ, &wsize);

	Packet_Reset(pktbuf);

	Packet_Push32(pktbuf, NET_WSIZE);
	Packet_Push32(pktbuf, 0);
	Packet_Push32(pktbuf, wsize.ws_col);
	Packet_Push32(pktbuf, wsize.ws_row);

	Client_SendPacket(Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));
}

/*-------------------- Poll -------------------------*/

void
Active_Poll()
{
	static int leaving_bytes = 4;

	if (activated == 0)
		return;

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

			fwrite(buf, 1, c, log);
			fflush(log);

			Packet_Reset(pktbuf);

			Packet_Push32(pktbuf, NET_DATA);
			Packet_Push32(pktbuf, 0);
			Packet_Push32(pktbuf, c);
			Packet_PushString(pktbuf, c, buf);

			Client_SendPacket(Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));

#if temp
			/* we look for the EOT in the last bytes of the buffer */
			if (buf[c - 1] == 4)
			{
				/* printf("FOUND ONE!!!\n"); */
				i = c;
				while (buf[--i] == 4)
				{
					/* printf("LEN %d removed one %d left char %d\n", c, leaving_bytes, buf[i]); */
					leaving_bytes--;
				}

				if (leaving_bytes <= 0)
				{
					TRACE("PIPE EXIT PACKET RECIEVED");
					Main_Exit();
					return;
				}
			}
			else
			{
				/* printf("LEN %d  %d \'%d\'\n", c, buf[c], '\r'); */
				leaving_bytes = 4;
				/* break; */
			}

			buffer = buf;

			t = 0;
			i = c;
			while (c > 0)
			{
				i = c;
				if (i > 490)
					i = 490;

				/* write an output to a file called log_file to debug the packets sent to the server. */
				/* fprintf(log, "c %d i %d t %d --> buffer \"%s\"\n", c, i, t, buffer); */
				fprintf(log, "c %d i %d t %d --> buffer \"", c, i, t);
				fwrite(buf, 1, i, log);
				fprintf(log, "\"\n");
				fflush(log);

				t += i;

				c -= i;

				Packet_Reset(pktbuf);

				Packet_Push32(pktbuf, NET_DATA);
				Packet_Push32(pktbuf, 0);
				Packet_Push32(pktbuf, i);
				Packet_PushString(pktbuf, i, buffer);

				if (Packet_GetLen(pktbuf) > (4 + 4 + i))
				{
					ERROR(Neuro_s("Packet bigger than it should by %d bytes", Packet_GetLen(pktbuf) - 200));
				}

				Client_SendPacket(Packet_GetBuffer(pktbuf), Packet_GetLen(pktbuf));
				buffer = &buf[t];
			}

			/* temporarily... this is not necessary */
			memset(buf, 0, t);
#endif /* temp */
		}

		if (c == -1 && errno != EAGAIN)
		{
			if (errno == EBADF || errno == EINTR)
			{
				/* end of process */
				Main_Exit();
				return;
			}
			else
			{
				printf(Neuro_s("read buffer empty, errno set %d\n", errno));
			}
		}
	}
}

/*-------------------- Constructor Destructor ----------------------*/

int
Active_Init(char *username, char *password)
{

	TRACE("Active Client initialization");

	pktbuf = Packet_Create();

	
	SendConnect(username, password, 1);

	return 0;
}

void
Active_Clean()
{
	Packet_Destroy(pktbuf);

	if (activated == 0)
		return;

	close(xfer_fifo[0]);
	close(xfer_fifo[1]);

	fclose(log);

	if (mpty)
	{
		close(mpty->master);
		close(mpty->slave);
		tcsetattr(0, TCSAFLUSH, &mpty->tt);
		free(mpty);
	}
}
