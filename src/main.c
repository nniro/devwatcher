/* main.c
 * Module : Main_
 */

/*-------------------- Extern Headers Including --------------------*/
#include <neuro/NEURO.h>
#include <neuro/nnet/network.h> /* NNet_SetDebugFilter() */
#include <stdio.h>

/*-------------------- Local Headers Including ---------------------*/
#include "core.h"

/*-------------------- Main Module Header --------------------------*/

#include "main.h"

/*--------------------      Other       ----------------------------*/

NEURO_MODULE_CHANNEL("main");

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

/* boolean flag for the application's running state : 
 * 0 == application set to stop 
 * 1 == it's running
 */
static u8 running = 1;

/*-------------------- Static Prototypes ---------------------------*/

static char *NAME;
static char *HOST;
static int PORT = 9000;
static char *PASSWORD;
static int CLIENT = 0;
static int LAYER = 0;

/*-------------------- Static Functions ----------------------------*/

static void
toconnect(char *data)
{
	if (data)
	{
		HOST = data;
	}
}

static void
set_port(char *data)
{
	if (data)
	{
		PORT = atoi(data);
	}
}

static void
set_name(char *data)
{
	if (data)
	{
		NAME = data;
	}
}

static void
set_password(char *data)
{
	if (data)
		PASSWORD = data;
}

static void
set_client(char *data)
{
	CLIENT = 1;
}

static void
set_layer(char *data)
{
	if (data)
	{
		LAYER = atoi(data);
	}
}

static void
outputhelp(char *data)
{
	printf("Usage: dwatcher [OPTIONS]... [SERVER]\n");
	printf("this program is a tool used to broadcast a shell to a server\n");
	printf("which can then be watched by other people.\n");
	printf("-h, --help		Displays this help\n");
	printf("-v, --version		Displays the version information.\n");
	printf("-a, --active		Sends terminal data to the server to be broadcasted.\n");
	printf("-p, --port		Sets the port the client will use to connect.\n");
	printf("-n, --name		Sets the name of the active client for a setting,\n");
	printf("			a query(passive) or a connection(passive).\n");
	printf("-l, --layer		as a passive client, connect to this layer\n");
	printf("-s, --password		Sets the password to be sent to the server when we\n");
	printf("			are an active client. And sets the server password when\n");
	printf("			we are a server.\n");
	printf("\n");
}

static void
outputversion(char *data)
{
	printf("dwatcher (developers watcher) %s\n", VERSION);
	printf("Written by Nicholas Niro\n\n");
	printf("Copyright (C) 2007 Neuroponic, Inc.\n");
	printf("This is free software; see the source for copying conditions. There is NO\n");
	printf("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
}

static int
init_modules()
{
	if (NNet_Init())
	{
		NEURO_ERROR("Initialisation of the neuronet library failed", NULL);
		return 1;
	}

	if (Core_Init())
	{
		NEURO_ERROR("Initialisation of the Core module, failed", NULL);
		return 1;
	}

	return 0;
}

static void
clean_modules()
{
	Core_Clean();
	NNet_Clean();
	Neuro_Quit();
}

/*-------------------- Global Functions ----------------------------*/

char *
Main_GetClientName()
{
	return NAME;
}

char *
Main_GetClientConnect()
{
	return HOST;
}

char *
Main_GetPassword()
{
	return PASSWORD;
}

int
Main_GetClientType()
{
	return CLIENT;
}

int
Main_GetPort()
{
	return PORT;
}

int
Main_GetLayer()
{
	return LAYER;
}

/*-------------------- Main Loop ----------------------------------------*/

void
main_loop()
{

	while (running)
	{
		if (Core_Poll())
			running = 0;

		Neuro_Sleep(5000);
	}	
}

/*-------------------- Main Entry ----------------------*/

int main(int argc, char **argv)
{
	int _err = 0;

	Neuro_ArgInit(argc, argv);

	Neuro_ArgOption("h,help", OPTION_QUIT, outputhelp);
	Neuro_ArgOption("v,version", OPTION_NORMAL | OPTION_QUIT, outputversion);
	Neuro_ArgOption(NULL, OPTION_NORMAL, toconnect);
	Neuro_ArgOption("p,port", OPTION_ARGUMENT, set_port);
	Neuro_ArgOption("n,name", OPTION_ARGUMENT, set_name);
	Neuro_ArgOption("l,layer", OPTION_ARGUMENT, set_layer);
	Neuro_ArgOption("s,password", OPTION_ARGUMENT, set_password);
	Neuro_ArgOption("a,active", OPTION_NORMAL, set_client);
	/*
	Neuro_ArgOption("s,summary", OPTION_NORMAL, set_summary);
	Neuro_ArgOption("d,description", OPTION_NORMAL, set_description);
	*/


	_err = Neuro_ArgProcess();

	NNet_SetDebugFilter("warn+all error+all");
	/* NNet_SetDebugFilter("all+all"); */
	Neuro_SetDebugFilter("+all trace-all");


	switch (_err)
	{
		case 0:
		{
			if (init_modules())
			{
				_err = 1;

				clean_modules();

				break;
			}
			
			main_loop();

			NEURO_TRACE("CLEANING MODULES", 0);
			clean_modules();
		}
		break;
		
		case 1:
		{
			_err = 0;
			/* printf("immediate exit\n"); */
			break;
		}
		break;

		case 2:
		{
			_err = 1;
			/* printf("error\n"); */
			break;
		}
		break;
		
		default:
		break;
	}

	Neuro_ArgClean();
	
	return _err;
}
/*-------------------- Main Exit ----------------------*/

void
Main_Exit()
{
	running = 0;
}
