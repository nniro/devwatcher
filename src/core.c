/* core.c
 * Module : Core_
 */

/*-------------------- Extern Headers Including --------------------*/
#include <neuro/NEURO.h>
#include <stdlib.h> /* getenv() */

/*-------------------- Local Headers Including ---------------------*/
#include "main.h"
#include "server.h"
#include "client.h"

/*-------------------- Main Module Header --------------------------*/
#include "core.h"


/*--------------------      Other       ----------------------------*/

NEURO_MODULE_CHANNEL("core");

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

static t_tick start_time;

/* set to 1 if we are a client */
static u8 client = 0;

/*-------------------- Static Prototypes ---------------------------*/



/*-------------------- Static Functions ----------------------------*/

/*-------------------- Global Functions ----------------------------*/

t_tick
Core_GetTime()
{
	return Neuro_GetTickCount() - start_time;
}

/*-------------------- Poll ----------------------------------------*/

int
Core_Poll()
{
	/* NEURO_TRACE("core cycle", NULL); */

	if (NNet_Poll())
		return 1;

	if (!client)
		Server_Poll();
	else
		Client_Poll();

	return 0;
}

/*-------------------- Constructor Destructor ----------------------*/

int
Core_Init()
{
	/* we buffer the start time of the program */
	start_time = Neuro_GetTickCount();


	if (Main_GetClientConnect())
	{
		char *name = Main_GetClientName();

		/* we are a client */
		NEURO_TRACE("We are a client %d", Main_GetClientType());

		if (name == NULL)
		{
			name = getenv("USER");

			NEURO_TRACE("Connecting as user %s\n", name);
		}

		if (Client_Init(name, Main_GetPassword(), 
					Main_GetClientConnect(), 
					Main_GetPort(),
					Main_GetLayer(),
					Main_GetClientType()))
		{
			return 1;
		}

		client = 1;
	}
	else
	{
		if (Main_GetClientType())
		{
			NEURO_ERROR("	To act as an active client, you need to input\n\
						the IP address of a running server and\n\
						also optionally a password.", NULL);
			return 1;
		}

		/* we are a server */
		NEURO_TRACE("We are a server", NULL);

		if (Server_Init(Main_GetPassword(), Main_GetPort()))
			return 1;
	}


	return 0;
}

void
Core_Clean()
{
	if (client)
		Client_Clean();
	else
		Server_Clean();
}