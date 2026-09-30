/*
 * Copyright (C) Cvitek Co., Ltd. 2019-2020. All rights reserved.
 *
 * File Name: custom_vencparam.c
 * Description:
 *   ....
 */
#include "custom_param.h"
#include "cvi_venc.h"

PARAM_CLASSDEFINE(PARAM_VENC_CHN_CFG_S,VENCCFG,CTX,VENC)[] = {
    {
        .stChnParam = {
            .bEnable = 1,
            .u8VencChn = 0,
            .enBindMode = VENC_BIND_VPSS,
            .astChn[0] = {
                .enModId = CVI_ID_VPSS,
                .s32DevId = 0,
                .s32ChnId = 0,
            },
            .astChn[1] = {
                .enModId = CVI_ID_VENC,
                .s32DevId = 0,
                .s32ChnId = 0,
            },
            .u8Profile = 0,
            .u16Width = 1920,
            .u16Height = 1080,
            .u8EsBufQueueEn = 1,
            .u16EnType = PT_H264,
            .u32BitStreamBufSize = 524288,
            .StreamTo = 1,
            .u8EntropyEncModeI = 0,
            .u8EntropyEncModeP = 0,
        },
        .stGopParam = {
            .u16gopMode = VENC_GOPMODE_NORMALP,
            .s8IPQpDelta = 0,
        },
        .stRcParam = {
            .u16Gop = 30,
            .u8SrcFrameRate = 15,
            .u8DstFrameRate = 15,
            .u16BitRate = 2000,
            .u8Qfactor = 60,
            .u32MaxBitRate = CVI_H26X_FRAME_BITS_DEFAULT,
            .u8VariFpsEn = 0,
            .u8StartTime = 2,
            .u16RcMode = VENC_RC_MODE_H264CBR,
            .u16FirstFrmstartQp = 30,
            .u16InitialDelay = CVI_INITIAL_DELAY_DEFAULT, // RW = , Range:[10, 3000] Rate control initial delay (ms).
            .u16ThrdLv = 2,/*RW = , Range:[0, 4] = , Mad threshold for controlling the macroblock-level bit rate */
            .u16BgDeltaQp = 0, /* RW = , Range:[-51, 51] =  Backgournd Qp Delta */
            .u8MinIprop = 1,
            .u8MaxIprop = 100,
            .u8MinIqp = CVI_H26X_MINQP_DEFAULT,//0-51
            .u8MaxIqp = CVI_H26X_MAXQP_DEFAULT,//0-51
            .u8MinQp = CVI_H26X_MINQP_DEFAULT,//0-51
            .u8MaxQp = CVI_H26X_MAXQP_DEFAULT,//0-51
            .u8MaxReEncodeTimes = 0, /* RW = , Range:[0, 3]  Range:max number of re-encode times.*/
            .u8QpMapEn = CVI_FALSE, /* RW = , Range:[0, 1]  enable qpmap.*/
            .u8ChangePos = DEF_26X_CHANGE_POS, //VBR使用
        },
    },
    {
        .stChnParam = {
            .bEnable = 1,
            .u8VencChn = 1,
            .enBindMode = VENC_BIND_VPSS,
            .astChn[0] = {
                .enModId = CVI_ID_VPSS,
                .s32DevId = 0,
                .s32ChnId = 1,
            },
            .astChn[1] = {
                .enModId = CVI_ID_VENC,
                .s32DevId = 0,
                .s32ChnId = 1,
            },
            .u8Profile = 0,
            .u16Width = 1280,
            .u16Height = 720,
            .u8EsBufQueueEn = 1,
            .u16EnType = PT_H264,
            .u32BitStreamBufSize = 524288,
            .StreamTo = 1,
            .u8EntropyEncModeI = 0,
            .u8EntropyEncModeP = 0,
        },
        .stGopParam = {
            .u16gopMode = VENC_GOPMODE_NORMALP,
            .s8IPQpDelta = 0,
        },
        .stRcParam = {
            .u16Gop = 30,
            .u8SrcFrameRate = 15,
            .u8DstFrameRate = 15,
            .u16BitRate = 2000,
            .u8Qfactor = 60,
            .u32MaxBitRate = CVI_H26X_FRAME_BITS_DEFAULT,
            .u8VariFpsEn = 0,
            .u8StartTime = 2,
            .u16RcMode = VENC_RC_MODE_H264CBR,
            .u16FirstFrmstartQp = 30,
            .u16InitialDelay = CVI_INITIAL_DELAY_DEFAULT, // RW = , Range:[10, 3000] Rate control initial delay (ms).
            .u16ThrdLv = 2,/*RW = , Range:[0, 4] = , Mad threshold for controlling the macroblock-level bit rate */
            .u16BgDeltaQp = 0, /* RW = , Range:[-51, 51] =  Backgournd Qp Delta */
            .u8MinIprop = 1,
            .u8MaxIprop = 100,
            .u8MinIqp = CVI_H26X_MINQP_DEFAULT,//0-51
            .u8MaxIqp = CVI_H26X_MAXQP_DEFAULT,//0-51
            .u8MinQp = CVI_H26X_MINQP_DEFAULT,//0-51
            .u8MaxQp = CVI_H26X_MAXQP_DEFAULT,//0-51
            .u8MaxReEncodeTimes = 0, /* RW = , Range:[0, 3]  Range:max number of re-encode times.*/
            .u8QpMapEn = CVI_FALSE, /* RW = , Range:[0, 1]  enable qpmap.*/
            .u8ChangePos = DEF_26X_CHANGE_POS, //VBR使用
        },
    },
    {
        .stChnParam = {
            .bEnable = 1,
            .u8VencChn = 2,
            .enBindMode = VENC_BIND_VPSS,
            .astChn[0] = {
                .enModId = CVI_ID_VPSS,
                .s32DevId = 0,
                .s32ChnId = 2,
            },
            .astChn[1] = {
                .enModId = CVI_ID_VENC,
                .s32DevId = 0,
                .s32ChnId = 2,
            },
            .u8Profile = 0,
            .u16Width = 640,
            .u16Height = 360,
            .u8EsBufQueueEn = 1,
            .u16EnType = PT_H264,
            .u32BitStreamBufSize = 524288,
            .StreamTo = 1,
            .u8EntropyEncModeI = 0,
            .u8EntropyEncModeP = 0,
        },
        .stGopParam = {
            .u16gopMode = VENC_GOPMODE_NORMALP,
            .s8IPQpDelta = 0,
        },
        .stRcParam = {
            .u16Gop = 30,
            .u8SrcFrameRate = 15,
            .u8DstFrameRate = 15,
            .u16BitRate = 2000,
            .u8Qfactor = 60,
            .u32MaxBitRate = CVI_H26X_FRAME_BITS_DEFAULT,
            .u8VariFpsEn = 0,
            .u8StartTime = 2,
            .u16RcMode = VENC_RC_MODE_H264CBR,
            .u16FirstFrmstartQp = 30,
            .u16InitialDelay = CVI_INITIAL_DELAY_DEFAULT, // RW = , Range:[10, 3000] Rate control initial delay (ms).
            .u16ThrdLv = 2,/*RW = , Range:[0, 4] = , Mad threshold for controlling the macroblock-level bit rate */
            .u16BgDeltaQp = 0, /* RW = , Range:[-51, 51] =  Backgournd Qp Delta */
            .u8MinIprop = 1,
            .u8MaxIprop = 100,
            .u8MinIqp = CVI_H26X_MINQP_DEFAULT,//0-51
            .u8MaxIqp = CVI_H26X_MAXQP_DEFAULT,//0-51
            .u8MinQp = CVI_H26X_MINQP_DEFAULT,//0-51
            .u8MaxQp = CVI_H26X_MAXQP_DEFAULT,//0-51
            .u8MaxReEncodeTimes = 0, /* RW = , Range:[0, 3]  Range:max number of re-encode times.*/
            .u8QpMapEn = CVI_FALSE, /* RW = , Range:[0, 1]  enable qpmap.*/
            .u8ChangePos = DEF_26X_CHANGE_POS, //VBR使用
        },
    }
};

PARAM_CLASSDEFINE(PARAM_ROI_PARAM_S,ROICFG,CTX,VENC)[] = {
    {
        .u8Index = 0,
        .bEnable = 0,
        .VencChn = 0,
        .bAbsQp = 1,
        .s32Qp = 20,
        .s32X = 10,
        .s32Y = 10,
        .u32Width = 256,
        .u32Height = 256,
    },
};

PARAM_VENC_CFG_S  g_stVencCtx = {
    .s32VencChnCnt = 3,
    .pstVencChnCfg = PARAM_CLASS(VENCCFG,CTX,VENC),
    .s8RoiNumber = 0,
    .pstRoiParam = PARAM_CLASS(ROICFG,CTX,VENC),
};

PARAM_VENC_CFG_S * PARAM_GET_VENC_CFG(void) {
    return &g_stVencCtx;
}
