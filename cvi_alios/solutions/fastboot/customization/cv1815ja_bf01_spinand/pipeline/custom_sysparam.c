/*
 * Copyright (C) Cvitek Co., Ltd. 2019-2020. All rights reserved.
 *
 * File Name: custom_sysparam.c
 * Description:
 *   ....
 */
#include "custom_param.h"
#include "board_config.h"

PARAM_CLASSDEFINE(PARAM_SBM_CFG_S,SBM,CTX,CFG)[] = {
    {
        .bEnable = 1,
        .s32SbmGrp = 0,
        .s32SbmChn = 0,
        .s32WrapBufLine = 64,
        .s32WrapBufSize = 16,
    },
    {
        .bEnable = 1,
        .s32SbmGrp = 3,
        .s32SbmChn = 0,
        .s32WrapBufLine = 64,
        .s32WrapBufSize = 16,
    },
    {
        .bEnable = 1,
        .s32SbmGrp = 1,
        .s32SbmChn = 0,
        .s32WrapBufLine = 64,
        .s32WrapBufSize = 16,
    },
};

PARAM_CLASSDEFINE(PARAM_VB_CFG_S,VBPOOL,CTX,VB)[] = {
    {
        .u16width = 2560,
        .u16height = 1440,
        .fmt = PIXEL_FORMAT_NV21,
        .enBitWidth = DATA_BITWIDTH_8,
        .enCmpMode = COMPRESS_MODE_NONE,
        .u8VbBlkCnt = 4,
    },
    {
        .u16width = 1920,
        .u16height = 1080,
        .fmt = PIXEL_FORMAT_NV21,
        .enBitWidth = DATA_BITWIDTH_8,
        .enCmpMode = COMPRESS_MODE_NONE,
        .u8VbBlkCnt = 4,
    },
    {
        .u16width = 1280,
        .u16height = 720,
        .fmt = PIXEL_FORMAT_NV21,
        .enBitWidth = DATA_BITWIDTH_8,
        .enCmpMode = COMPRESS_MODE_NONE,
        .u8VbBlkCnt = 4,
    },
    {
        .u16width = 640,
        .u16height = 360,
        .fmt = PIXEL_FORMAT_NV21,
        .enBitWidth = DATA_BITWIDTH_8,
        .enCmpMode = COMPRESS_MODE_NONE,
        .u8VbBlkCnt = 4,
    },
};

PARAM_SYS_CFG_S  g_stSysCtx = {
    .u8SbmCnt = 0,
    .pstSbmCfg = PARAM_CLASS(SBM,CTX,CFG),
    .stSwitchCfg.bMipiSwitchEnable = 0,
    .stSwitchCfg.u32MipiSwitchGpioIdx = 4,
    .stSwitchCfg.u32MipiSwitchGpio = 22,
    .stSwitchCfg.bMipiSwitchPull = 1,
    .stSwitchCfg.u32SwitchPipe0 = 0,
    .stSwitchCfg.u32SwitchPipe1 = 3,
    .u8VbPoolCnt = 4,
    .pstVbPool = PARAM_CLASS(VBPOOL,CTX,VB),
    .stVIVPSSMode.aenMode[0] = VI_OFFLINE_VPSS_OFFLINE,
    .stVIVPSSMode.aenMode[1] = VI_OFFLINE_VPSS_OFFLINE,
    .stVPSSMode.enMode = VPSS_MODE_DUAL,
    .stVPSSMode.aenInput[0] = VPSS_INPUT_MEM,
    .stVPSSMode.aenInput[1] = VPSS_INPUT_MEM,
};

PARAM_SYS_CFG_S * PARAM_GET_SYS_CFG(void) {
    return &g_stSysCtx;
}
