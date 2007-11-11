/* core.c
 * Module : Core_
 */

/*-------------------- Extern Headers Including --------------------*/
#include <neuro/NEURO.h>

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
		NEURO_TRACE("We are a client", NULL);

		if (name == NULL)
			name = "default-client";

		if (Client_Init(name, Main_GetClientConnect(), Main_GetPort()))
			return 1;

		client = 1;
	}
	else
	{
		/* we are a server */
		NEURO_TRACE("We are a server", NULL);

		if (Server_Init(Main_GetPort()))
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
