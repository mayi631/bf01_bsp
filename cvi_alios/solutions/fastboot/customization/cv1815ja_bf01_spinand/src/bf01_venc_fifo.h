#ifndef BF01_VENC_FIFO_H
#define BF01_VENC_FIFO_H

#include <stddef.h>
#include "cvi_venc.h"

typedef unsigned long CVI_DATAFIFO_HANDLE;

typedef enum {
    DATAFIFO_READER,
    DATAFIFO_WRITER
} CVI_DATAFIFO_OPEN_MODE_E;

typedef struct {
    CVI_U32 u32EntriesNum;
    CVI_U32 u32CacheLineSize;
    CVI_BOOL bDataReleaseByWriter;
    CVI_DATAFIFO_OPEN_MODE_E enOpenMode;
} CVI_DATAFIFO_PARAMS_S;

typedef enum {
    DATAFIFO_CMD_GET_PHY_ADDR,
    DATAFIFO_CMD_READ_DONE,
    DATAFIFO_CMD_WRITE_DONE,
    DATAFIFO_CMD_SET_DATA_RELEASE_CALLBACK,
    DATAFIFO_CMD_GET_AVAIL_WRITE_LEN,
    DATAFIFO_CMD_GET_AVAIL_READ_LEN,
    DATAFIFO_CMD_SHOW_POINTER
} CVI_DATAFIFO_CMD_E;

#define BF01_VENC_FIFO_PACKS 8

struct bf01_venc_fifo_stream {
    VENC_PACK_S packs[BF01_VENC_FIFO_PACKS];
    VENC_STREAM_S stream;
    VENC_CHN channel;
};

_Static_assert(sizeof(CVI_DATAFIFO_PARAMS_S) == 16, "Unexpected SDK DATAFIFO parameters ABI");
_Static_assert(offsetof(struct bf01_venc_fifo_stream, stream) == 1152, "Unexpected SDK VENC packs ABI");
_Static_assert(offsetof(struct bf01_venc_fifo_stream, channel) == 1536, "Unexpected SDK VENC stream ABI");
_Static_assert(sizeof(struct bf01_venc_fifo_stream) == 1544, "Unexpected SDK VENC FIFO ABI");

CVI_S32 CVI_VENC_GetDataFifoAddr(VENC_CHN channel, CVI_U64 *address);
CVI_S32 CVI_DATAFIFO_OpenByAddr(CVI_DATAFIFO_HANDLE *handle, CVI_DATAFIFO_PARAMS_S *parameters,
    CVI_U64 address);
CVI_S32 CVI_DATAFIFO_CMD(CVI_DATAFIFO_HANDLE handle, CVI_DATAFIFO_CMD_E command, CVI_VOID *argument);
CVI_S32 CVI_DATAFIFO_Read(CVI_DATAFIFO_HANDLE handle, CVI_VOID **data);
CVI_S32 CVI_DATAFIFO_Close(CVI_DATAFIFO_HANDLE handle);

#endif
