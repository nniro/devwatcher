/* util.h */

#ifndef __UTIL_H
#define __UTIL_H

/* Do a sum of a data buffer
 *
 * if mOutput is NULL, return a statically linked variable 
 *
 * Don't Free the static output!
 */
extern unsigned char *sha1sum(const unsigned char *input, int len, unsigned char *mOutput);

#endif /* NOT __UTIL_H */
