/*
 * Copyright (C) Cvitek Co., Ltd. 2019-2020. All rights reserved.
 *
 * File Name: custom_vpsscfg.c
 * Description:
 *   ....
 */
#include "custom_param.h"
#include "board_config.h"
#include "cvi_buffer.h"
PARAM_CLASSDEFINE(PARAM_VPSS_CHN_CFG_S,CHNCFG,GRP0,CHN)[] = {
    {
        .abChnEnable = 1,
        .stVpssChnAttr = {
            .u32Width = 1920,
            .u32Height = 1080,
            .enVideoFormat = VIDEO_FORMAT_LINEAR,
            .enPixelFormat = PIXEL_FORMAT_NV21,
            .stFrameRate = {
                .s32SrcFrameRate = 30,
                .s32DstFrameRate = 15,
            },
            .u32Depth = 0,
            .bMirror = 0,
            .bFlip = 0,
            .stAspectRatio = {
                .enMode = ASPECT_RATIO_NONE,
                .stVideoRect = {
                    .s32X = 0,
                    .s32Y = 0,
                    .u32Width = 0,
                    .u32Height = 0,
                },
                .bEnableBgColor = 0,
                .u32BgColor = 0,
                
            },
            .stNormalize = {
                .bEnable = 0,
            },
        },
        .stVpssChnCropInfo = {
            .bEnable = 0,
            .enCropCoordinate = VPSS_CROP_RATIO_COOR,
            .stCropRect = {
                .s32X = 0,
                .s32Y = 0,
                .u32Width = 0,
                .u32Height = 0,
            },
        },
        .u8VpssAttachEnable = 1,
        .u8VpssAttachId = 1,
        .u8Rotation = ROTATION_0,
    },
    {
        .abChnEnable = 1,
        .stVpssChnAttr = {
            .u32Width = 1280,
            .u32Height = 720,
            .enVideoFormat = VIDEO_FORMAT_LINEAR,
            .enPixelFormat = PIXEL_FORMAT_NV21,
            .stFrameRate = {
                .s32SrcFrameRate = 30,
                .s32DstFrameRate = 15,
            },
            .u32Depth = 0,
            .bMirror = 0,
            .bFlip = 0,
            .stAspectRatio = {
                .enMode = ASPECT_RATIO_NONE,
                .stVideoRect = {
                    .s32X = 0,
                    .s32Y = 0,
                    .u32Width = 0,
                    .u32Height = 0,
                },
                .bEnableBgColor = 0,
                .u32BgColor = 0,
                
            },
            .stNormalize = {
                .bEnable = 0,
            },
        },
        .stVpssChnCropInfo = {
            .bEnable = 0,
            .enCropCoordinate = VPSS_CROP_RATIO_COOR,
            .stCropRect = {
                .s32X = 0,
                .s32Y = 0,
                .u32Width = 0,
                .u32Height = 0,
            },
        },
        .u8VpssAttachEnable = 1,
        .u8VpssAttachId = 2,
        .u8Rotation = ROTATION_0,
    },
    {
        .abChnEnable = 1,
        .stVpssChnAttr = {
            .u32Width = 640,
            .u32Height = 360,
            .enVideoFormat = VIDEO_FORMAT_LINEAR,
            .enPixelFormat = PIXEL_FORMAT_NV21,
            .stFrameRate = {
                .s32SrcFrameRate = 30,
                .s32DstFrameRate = 15,
            },
            .u32Depth = 0,
            .bMirror = 0,
            .bFlip = 0,
            .stAspectRatio = {
                .enMode = ASPECT_RATIO_NONE,
                .stVideoRect = {
                    .s32X = 0,
                    .s32Y = 0,
                    .u32Width = 0,
                    .u32Height = 0,
                },
                .bEnableBgColor = 0,
                .u32BgColor = 0,
                
            },
            .stNormalize = {
                .bEnable = 0,
            },
        },
        .stVpssChnCropInfo = {
            .bEnable = 0,
            .enCropCoordinate = VPSS_CROP_RATIO_COOR,
            .stCropRect = {
                .s32X = 0,
                .s32Y = 0,
                .u32Width = 0,
                .u32Height = 0,
            },
        },
        .u8VpssAttachEnable = 1,
        .u8VpssAttachId = 3,
        .u8Rotation = ROTATION_0,
    }
};

PARAM_CLASSDEFINE(PARAM_VPSS_GRP_CFG_S,GRPCFG,CTX,GRP)[] = {
    {
        .bEnable = 1,
        .stVpssGrpAttr = {
            .enPixelFormat = PIXEL_FORMAT_NV21,
            .stFrameRate = {
                .s32SrcFrameRate = -1,
                .s32DstFrameRate = -1,
            },
            .u8VpssDev = 1,
            .u32MaxW = 2560,
            .u32MaxH = 1440,
        },
        .VpssGrp = 0,
        .u8ChnCnt = 3,
        .pstChnCfg = PARAM_CLASS(CHNCFG,GRP0,CHN),
        .stVpssGrpCropInfo = {
            .bEnable = 0,
            .enCropCoordinate = VPSS_CROP_RATIO_COOR,
            .stCropRect = {
                .s32X = 0,
                .s32Y = 0,
                .u32Width = 0,
                .u32Height = 0,
            },
        },
        .bBindMode = 1,
        .astChn[0].enModId = CVI_ID_VI,
        .astChn[0].s32DevId = 0,
        .astChn[0].s32ChnId = 0,
        .astChn[1].enModId = CVI_ID_VPSS,
        .astChn[1].s32DevId = 0,
        .astChn[1].s32ChnId = 0,
        .s32BindVidev = 0,
        .u8ViRotation = 0,
    }
};

PARAM_VPSS_CFG_S  g_stVpssCtx = {
    .u8GrpCnt = 1,
    .pstVpssGrpCfg = PARAM_CLASS(GRPCFG,CTX,GRP),
};

PARAM_VPSS_CFG_S * PARAM_GET_VPSS_CFG(void) {
    return &g_stVpssCtx;
}
