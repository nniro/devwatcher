/* core.c
 * Module : Core_
 */

/*-------------------- Extern Headers Including --------------------*/
#include <neuro/NEURO.h>
#include <neuro/network.h>
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

static t_tick start_time = 0;

/* set to 1 if we are a client */
static u8 client = 0;

static NNET_MASTER *master = NULL;

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
	NNET_STATUS *status = NULL;
	/* NEURO_TRACE("core cycle", NULL); */
	status = NNet_Poll(master);

	if (!status)
	{
		NEURO_ERROR("Status is NULL", NULL);
		return 1;
	}

	if (!client)
		return Server_Poll(status);
	else
		return Client_Poll(status);

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

		master = NNet_Create(TYPE_CLIENT);

		/* we are a client */
		NEURO_TRACE("We are a client %d", Main_GetClientType());

		if (name == NULL)
		{
			name = getenv("USER");

			NEURO_TRACE("Connecting as user %s\n", name);
		}

		if (Client_Init(master, name, Main_GetPassword(), 
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

		master = NNet_Create(TYPE_SERVER);

		if (Server_Init(master, Main_GetPassword(), Main_GetPort()))
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

	NNet_Destroy(master);
}
