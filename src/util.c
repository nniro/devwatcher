/* util.c
 * Module : Util
 */

/*-------------------- Extern Headers Including --------------------*/
#include <openssl/sha.h> /* SHA1 */

#include <stdlib.h>
#include <stdio.h> /* sprintf */

#include <string.h> /* strlen */

/*-------------------- Local Headers Including ---------------------*/

/*-------------------- Main Module Header --------------------------*/
#include "util.h"


/*--------------------      Other       ----------------------------*/

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

/* pretty hash result array string */
static unsigned char hashResult[41];

/*-------------------- Static Prototypes ---------------------------*/



/*-------------------- Static Functions ----------------------------*/

/*-------------------- Global Functions ----------------------------*/

/* Do a sum of a data buffer
 *
 * if mOutput is NULL, return a statically linked variable 
 *
 * Don't Free the static output!
 */
unsigned char *
sha1sum(const unsigned char *input, int len, unsigned char *mOutput)
{
	unsigned char *raw;
	int i = 0;
	int j = 0;
	unsigned char *output;
	
	if (!mOutput)
		output = &hashResult[0];
	else
		output = mOutput;

	raw = SHA1(input, len, NULL);

	if (!raw)
		return NULL;

	while (i < 20) /* the result of SHA1 is always 20 bytes */
	{
		int t = 0;
		if ((int)raw[i] < 0)
		{
			t = abs(raw[i]);
			t = t ^ 0x80; /* this fixes negative integers to a proper 8bit value */
		}
		else
			t = raw[i];
		sprintf((char*)&output[j], "%2x", t);
		j += 2;
		i++;
	}
	/* the ending NULL */
	output[j] = '\0';

	return output;
}

