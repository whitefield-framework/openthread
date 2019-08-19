#ifndef _WHITEFIELD_H_
#define _WHITEFIELD_H_

int wfInit(int nodeid);
void wfDeinit(void);
void wfSendPacket(uint8_t *buf, size_t len);

#endif  //_WHITEFIELD_H_
