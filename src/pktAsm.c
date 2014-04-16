/* pktAsm.c
 * Module : PktAsm
 */

/*-------------------- Extern Headers Including --------------------*/

#include <neuro/NEURO.h>

#include <stdlib.h>

#include <string.h> /* memcpy */

/*-------------------- Local Headers Including ---------------------*/
#include "global.h"

/*-------------------- Main Module Header --------------------------*/
#include "pktAsm.h"


/*--------------------      Other       ----------------------------*/

NEURO_MODULE_CHANNEL("pktAsm");

#define MEMOVERHEAD 1024

struct PktAsm
{
	char *data;
	int curSize;
	int totalSize;

	/* memory available in data */
	int mem;
	/* the size of the memory in data */
	int totalMem;
};

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

/*-------------------- Static Prototypes ---------------------------*/



/*-------------------- Static Functions ----------------------------*/

/*-------------------- Global Functions ----------------------------*/

/* 
 * returns -1 on error
 * returns 0 if need to feed more data
 * returns 1 if it finished processing all packets and doesn't
 * 			have any outputRemainData or outputRemainLen
 * returns 2 if it finished processing the current packet but has
 * 			more in outputRemainData and outputRemainLen
 *
 * outputRemainData has to be checked if not NULL everytime and outputRemainLen
 * if they aren't NULL, it means a new packet was started so PktAsm_Push has
 * to be called with those new information.
 */
int
PktAsm_Process(PktAsm *pa, int (*getPacketSize)(const char *data), char *data, int len, char **outputData, int *outputTotalSize, char **outputRemainData, int *outputRemainLen)
{
	/*
	 * len == 100
	 * curSize = 300
	 * totalSize = 500
	 *
	 * len == 100
	 * curSize = 400
	 * totalSize = 500
	 *
	 * len == 250
	 * curSize = 450
	 * totalSize = 500
	 *
	 * lenLeft = totalSize - curSize = 50
	 * outputRemainLen = len - lenLeft = 200
	 *
	 */
	int result = 0;
	int tmpLen = 0;

	if (!pa || !data)
	{
		ERROR("Invalid argument passed to function");
		return -1;
	}

	if (len == 0)
		return 1;

	/* TRACE(Neuro_s("Got packet of length : %d (cur %d, total %d)", len, pa->curSize, pa->totalSize)); */

	if (pa->totalSize == 0)
	{
		/* special, this is a start of a packet 
		 * so we get the special header total packet
		 * length value.
		 */

		if (!getPacketSize)
		{
			if (len < sizeof(int))
			{
				ERROR("Invalid packet not having a correct header with the size");
				return -1;
			}

			memcpy(&pa->totalSize, data, sizeof(int));

			if (pa->totalSize < 0)
			{
				ERROR("Invalid packet not having a valid size");
				return -1;
			}

			/* TRACE(Neuro_s("Packet header size %d", pa->totalSize)); */

			data = &data[sizeof(int)];

			len -= sizeof(int);
		}
		else
			pa->totalSize = (getPacketSize)(data);

		TRACE(Neuro_s("Packet assembly process, packet len %d, header packet size %d", len, pa->totalSize));

		if (pa->totalSize < 0)
		{
			ERROR("Invalid packet not having a valid size");
			return -1;
		}

		if (pa->totalSize == 0)
			return -1;

		/*
		if (pa->totalSize < len)
		{
			ERROR("This packet may be an attempt to forge a packet");
			return -1;
		}
		*/
	}

	if (len > pa->totalSize - pa->curSize)
	{
		/* we have another packet in this */
		*outputRemainLen = len - (pa->totalSize - pa->curSize);
		*outputRemainData = &data[pa->totalSize];

		tmpLen = pa->totalSize - pa->curSize;

		result = 1;
	}

	if (len <= pa->totalSize - pa->curSize)
		tmpLen = len;

	if (pa->mem < tmpLen)
	{
		pa->data = realloc(pa->data, tmpLen + (pa->totalMem + (sizeof(char) * MEMOVERHEAD)));
		pa->mem = tmpLen + (sizeof(char) * MEMOVERHEAD);
		pa->totalMem += pa->mem;
	}

	memcpy(&pa->data[pa->curSize], data, tmpLen);
	pa->mem -= tmpLen;
	pa->curSize += tmpLen;

	if (pa->curSize == pa->totalSize)
	{
		*outputData = pa->data;
		*outputTotalSize = pa->totalSize;

		pa->mem = pa->totalMem;
		pa->curSize = 0;
		pa->totalSize = 0;

		return result + 1;
	}

	return result;
}

/*-------------------- Constructor Destructor ----------------------*/

PktAsm *
PktAsm_Create(void)
{
	PktAsm *result = NULL;

	result = malloc(sizeof(PktAsm));

	result->data = malloc(sizeof(char) * MEMOVERHEAD);
	result->mem = sizeof(char) * MEMOVERHEAD;
	result->totalMem = result->mem;

	result->curSize = 0;
	result->totalSize = 0;

	return result;
}

void
PktAsm_Destroy(PktAsm *pa)
{
	if (pa)
	{
		if (pa->totalMem > 0)
			free(pa->data);
		free(pa);
	}
}
