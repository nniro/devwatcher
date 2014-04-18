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
	char *data; /* the buffer containing the packet in making */

	int curSize; /* the current size of the data that we gathered so far. */
	int totalSize; /* the size the packet has to be at the end, when we got
			* all its pieces.
       			*/

	/* the goal is to attain curSize == totalSize */

	/* when bytes are copied to the buffer data, this variable 
	 * contains how much memory is left free in data.
	 *
	 */
	int mem;
	/* the total allocated bytes available in the buffer data
	 *
	 * FIXME Can't we deduce this by doing mem + totalSize?
	 */
	int totalMem;
};

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

/*-------------------- Static Prototypes ---------------------------*/



/*-------------------- Static Functions ----------------------------*/

/*-------------------- Global Functions ----------------------------*/

void
PktAsm_Reset(PktAsm *pa)
{
	if (!pa)
		return;

	pa->curSize = 0;
	pa->totalSize = 0;

	pa->mem = pa->totalMem;
}

/* 
 * returns -1 on error
 * returns 0 if need to feed more data
 * returns 1 if it finished processing all packets and doesn't
 * 			have any outputRemainData or outputRemainLen
 * returns 2 if it finished processing the current packet but has
 * 			more in outputRemainData and outputRemainLen
 * returns 3 if it couldn't do anything and returned nothing.
 *
 * outputRemainData has to be checked if not NULL everytime and outputRemainLen
 * if they aren't NULL, it means a new packet was started so PktAsm_Push has
 * to be called with those new information.
 */
int
PktAsm_Process(PktAsm *pa, int (*getPacketSize)(const char *data), const char *data, int len, char **outputData, int *outputTotalSize, char **outputRemainData, int *outputRemainLen)
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

	if (pa->curSize == pa->totalSize && pa->totalSize > 0)
		return 1; /* the packet is a whole already */

	if (len == 0)
		return 3; /* can't do anything */

	/* TRACE(Neuro_s("Got packet of length : %d (cur %d, total %d)", len, pa->curSize, pa->totalSize)); */

	if (pa->totalSize == 0)
	{
		/* special, this is a start of a packet 
		 * so we get the special header total packet
		 * length value.
		 */

		if (!getPacketSize)
		{
			/* there is no method to know the size of the packet
			 * so we assume that the first 4 bytes of the string is the
			 * size of the packet.
			 */

			if (len < sizeof(int))
			{
				ERROR("Invalid packet not having a correct header with the size");
				return -1;
			}

			/* we copy the first 4 bytes to the totalSize */
			memcpy(&pa->totalSize, data, sizeof(int));

			if (pa->totalSize < 0)
			{
				ERROR("Invalid packet not having a valid size");
				return -1;
			}

			/* TRACE(Neuro_s("Packet header size %d", pa->totalSize)); */

			/* we move the pointer past the first integer (the size) */
			data = &data[sizeof(int)];

			/* we remove the integer size from the length */
			len -= sizeof(int);

			TRACE(Neuro_s("without the callback getPacketSize, totalSize = %d", pa->totalSize));
		}
		else
		{
			pa->totalSize = (getPacketSize)(data);
		}

		TRACE(Neuro_s("Packet assembly process, packet len %d, header packet size %d", len, pa->totalSize));

		if (pa->totalSize < 0)
		{
			ERROR("Invalid packet not having a valid size");
			return -1;
		}

		if (pa->totalSize == 0)
			return -1;

		TRACE(Neuro_s("Starting to assemble a new packet of size %d", pa->totalSize));
	}
	else
	{
		TRACE(Neuro_s("Continuing packet at : %d/%d with new len %d", pa->curSize, pa->totalSize, len));
	}

	if (len > pa->totalSize - pa->curSize)
	{
		/* we have another packet in this */
		*outputRemainLen = len - (pa->totalSize - pa->curSize);
		*outputRemainData = &data[pa->totalSize - pa->curSize];

		tmpLen = pa->totalSize - pa->curSize;

		result = 1;
	}

	if (len <= pa->totalSize - pa->curSize)
		tmpLen = len;

	if (pa->mem < tmpLen)
	{
		pa->data = realloc(pa->data, (tmpLen - pa->mem) + (pa->totalMem + (sizeof(char) * MEMOVERHEAD)));
		pa->totalMem = (tmpLen - pa->mem) + (pa->totalMem + (sizeof(char) * MEMOVERHEAD));
		pa->mem += (tmpLen - pa->mem) + (sizeof(char) * MEMOVERHEAD);
		/*pa->mem = tmpLen + (sizeof(char) * MEMOVERHEAD);*/
		/* pa->totalMem += pa->mem; */
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
