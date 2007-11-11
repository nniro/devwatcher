/* packet.h */

#ifndef __PACKET_H
#define __PACKET_H

#include <neuro/NEURO.h>

#include "global.h"

typedef struct Packet Packet;

extern int Packet_Push64(Packet *pkt, const double num);
extern int Packet_Push32(Packet *pkt, const u32 num);
extern int Packet_Push16(Packet *pkt, const u16 num);
extern int Packet_Push8(Packet *pkt, const u8 num);

extern int Packet_PushStruct(Packet *pkt, const u32 len, const void *stru);
extern int Packet_PushString(Packet *pkt, const u32 len, const char *string);

extern int Packet_GetLen(const Packet *pkt);
extern char *Packet_GetBuffer(const Packet *pkt);

extern void Packet_Reset(Packet *pkt);

extern Packet *Packet_Create();
extern void Packet_Destroy(Packet *pkt);

#endif /* NOT __PACKET_H */
