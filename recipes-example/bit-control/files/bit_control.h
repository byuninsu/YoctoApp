#ifndef BIT_CONTROL_H
#define BIT_CONTROL_H

#define GPU_API

#include <stdint.h>

uint32_t GPU_API WriteBitErrorData(uint32_t *mItemStates, uint32_t mtype);
cJSON* GPU_API ReadBitErrorLog(const char* date);

#endif // BIT_CONTROL_H
