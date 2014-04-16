/* pktAsm.h */

#ifndef __PKTASM_H
#define __PKTASM_H

typedef struct PktAsm PktAsm;

extern int PktAsm_Process(PktAsm *pa, int (*getPacketSize)(const char *data), char *data, int len, char **outputData, int *outputTotalSize, char **outputRemainData, int *outputRemainLen);

extern PktAsm *PktAsm_Create(void);
extern void PktAsm_Destroy(PktAsm *pa);

#endif /* NOT __PKTASM_H */
