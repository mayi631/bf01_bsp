/*
 * Copyright (C) Cvitek Co., Ltd. 2019-2020. All rights reserved.
 *
 * File Name: custom_viparam.c
 * Description:
 *   BF01 / GC4683 single-sensor VI params.
 */
#include "custom_param.h"

PARAM_CLASSDEFINE(PARAM_SNS_CFG_S,SENSORCFG,CTX,Sensor)[] = {
    {
        .s32Framerate = 60,
        .enSnsType = GCORE_GC4683_MIPI_4M_60FPS_10BIT,
        .MipiDev = 0,
        .s32BusId = 2,
        .s8I2cDev = 2,
        .s32I2cAddr = 0x31,
        .as16LaneId = {0, 1, 2, 3, 4},
        .as8PNSwap = {0, 0, 0, 0, 0},
        .bMclkEn = 1,
        .u8Mclk = RX_MAC_CLK_600M,
        .u8MclkFreq = CAMPLL_FREQ_24M,
        .u8MclkCam = 0,
        .u8Orien = 0,
        .bHwSync = 0,
        .u32Rst_port_idx = 0, /* GPIOA */
        .u32Rst_pin = 2,      /* XGPIOA[2] */
        .u32Rst_pol = OF_GPIO_ACTIVE_LOW,
        .u8Rotation = 0,
    },
};

PARAM_CLASSDEFINE(PARAM_ISP_CFG_S,ISPCFG,CTX,ISP)[] = {
    {
        .bMonoSet = {0},
        .bUseSingleBin = 0,
        .stPQBinDes[0] =
        {
            .pIspBinData = NULL,
            .u32IspBinDataLen = 0,
        },
        .s8ByPassNum = 5,
    },
};

PARAM_CLASSDEFINE(PARAM_CHN_CFG_S,CHNCFG,CTX,CHN)[] = {
    {
        .s32ChnId = 0,
        .enWDRMode = WDR_MODE_NONE,
        .bYuvBypassPath = 0,
        .f32Fps = -1,
        .u32Width = 2560,
        .u32Height = 1440,
        .enPixFormat = PIXEL_FORMAT_NV21,
        .enDynamicRange = DYNAMIC_RANGE_SDR8,
        .enVideoFormat = VIDEO_FORMAT_LINEAR,
        .enCompressMode = COMPRESS_MODE_TILE,
    },
};

PARAM_CLASSDEFINE(PARAM_PIPE_CFG_S,PIPECFG,CTX,PIPE)[] = {
    {
        .pipe = {0, -1, -1, -1, -1, -1},
    },
};

PARAM_VI_CFG_S g_stViCtx = {
    .u32WorkSnsCnt = 1,
    .pstSensorCfg = PARAM_CLASS(SENSORCFG,CTX,Sensor),
    .pstPipeInfo = PARAM_CLASS(PIPECFG,CTX,PIPE),
    .pstIspCfg = PARAM_CLASS(ISPCFG,CTX,ISP),
    .pstChnInfo = PARAM_CLASS(CHNCFG,CTX,CHN),
};

PARAM_VI_CFG_S * PARAM_GET_VI_CFG(void) {
    return &g_stViCtx;
}
