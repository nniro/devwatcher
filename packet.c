/* packet.c
 * Module : Packet_
 */

/*-------------------- Extern Headers Including --------------------*/

#include <neuro/NEURO.h>
#include <string.h> /* memcpy() */

/*-------------------- Local Headers Including ---------------------*/

/*-------------------- Main Module Header --------------------------*/

#include "packet.h"

/*--------------------      Other       ----------------------------*/

NEURO_MODULE_CHANNEL("packet");

struct Packet
{
        u32 len; /* length of the memory used */
	u32 mem; /* available free memory */
        char *buffer; /* the actual buffer */
};

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

/*-------------------- Static Prototypes ---------------------------*/

/*-------------------- Static Functions ----------------------------*/

static int
push_data(Packet *pkt, const u32 len, const char *data)
{
	if (!pkt)
	{
		NEURO_WARN("Packet type is NULL", NULL);
		return 1;
	}

	if (!data)
	{
		NEURO_WARN("Input data pointer is NULL", NULL);
		return 1;
	}

	if (len == 0)
	{
		NEURO_WARN("data lenght is 0", NULL);
		return 1;
	}

	if (len > pkt->mem)
	{
		pkt->buffer = realloc(pkt->buffer, pkt->len + pkt->mem + (len - pkt->mem));
		pkt->mem += (len - pkt->mem);
	}

	memcpy(&pkt->buffer[pkt->len], data, len);

	pkt->mem -= len;
	pkt->len += len;

	return 0;
}

/*-------------------- Global Functions ----------------------------*/

int
Packet_Push64(Packet *pkt, const double num)
{
	char *buf;

	buf = (char *)&num;

	return push_data(pkt, 8, buf);
}

int
Packet_Push32(Packet *pkt, const u32 num)
{
	char *buf;
	
	buf = (char *)&num;

	return push_data(pkt, 4, buf);
}

int
Packet_Push16(Packet *pkt, const u16 num)
{
	char *buf;

	buf = (char *)&num;

	return push_data(pkt, 2, buf);
}

int
Packet_Push8(Packet *pkt, const u8 num)
{

	return push_data(pkt, 1, (char*)&num);
}

int
Packet_PushStruct(Packet *pkt, const u32 len, const void *stru)
{
	return push_data(pkt, len, (char*)stru);
}

int
Packet_PushString(Packet *pkt, const u32 len, const char *string)
{
	return push_data(pkt, len, string);
}

int
Packet_GetLen(const Packet *pkt)
{
	if (!pkt)
	{
		NEURO_WARN("Packet type is NULL", NULL);
		return -1;
	}

	return pkt->len;
}

char *
Packet_GetBuffer(const Packet *pkt)
{
	if (!pkt)
	{
		NEURO_WARN("Packet type is NULL", NULL);
		return NULL;
	}

	return pkt->buffer;
}

/*-------------------- Constructor Destructor ----------------------*/

void
Packet_Reset(Packet *pkt)
{
	if (!pkt)
	{
		NEURO_WARN("Packet type is NULL", NULL);
		return;
	}
	
	memset(pkt->buffer, 0, pkt->len);

	pkt->mem += pkt->len;

	pkt->len = 0;

}

Packet *
Packet_Create()
{
	Packet *output;

	output = calloc(1, sizeof(Packet));


	/* we allocate 20 bytes of overhead to accelerate the process */
	output->buffer = calloc(1, 20);
	/* we keep that amount of available memory in this variable */
	output->mem = 20;


	return output;
}

void
Packet_Destroy(Packet *pkt)
{
	if (pkt)
	{
		if (pkt->buffer)
			free(pkt->buffer);

		free(pkt);
	}
}
