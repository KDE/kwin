/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWINEGLEXT_H
#define KWINEGLEXT_H

#include <EGL/eglext.h>

#ifndef EGL_WL_bind_wayland_display
#define EGL_WAYLAND_BUFFER_WL                     0x31D5
#define EGL_WAYLAND_PLANE_WL                      0x31D6
#define EGL_TEXTURE_Y_U_V_WL                      0x31D7
#define EGL_TEXTURE_Y_UV_WL                       0x31D8
#define EGL_TEXTURE_Y_XUXV_WL                     0x31D9
#define EGL_TEXTURE_EXTERNAL_WL                   0x31DA
#define EGL_WAYLAND_Y_INVERTED_WL                 0x31DB
#endif // EGL_WL_bind_wayland_display

#ifndef EGL_EXT_image_dma_buf_import
#define EGL_LINUX_DMA_BUF_EXT                     0x3270
#define EGL_LINUX_DRM_FOURCC_EXT                  0x3271
#define EGL_DMA_BUF_PLANE0_FD_EXT                 0x3272
#define EGL_DMA_BUF_PLANE0_OFFSET_EXT             0x3273
#define EGL_DMA_BUF_PLANE0_PITCH_EXT              0x3274
#define EGL_DMA_BUF_PLANE1_FD_EXT                 0x3275
#define EGL_DMA_BUF_PLANE1_OFFSET_EXT             0x3276
#define EGL_DMA_BUF_PLANE1_PITCH_EXT              0x3277
#define EGL_DMA_BUF_PLANE2_FD_EXT                 0x3278
#define EGL_DMA_BUF_PLANE2_OFFSET_EXT             0x3279
#define EGL_DMA_BUF_PLANE2_PITCH_EXT              0x327A
#define EGL_YUV_COLOR_SPACE_HINT_EXT              0x327B
#define EGL_SAMPLE_RANGE_HINT_EXT                 0x327C
#define EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT 0x327D
#define EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT   0x327E
#define EGL_ITU_REC601_EXT                        0x327F
#define EGL_ITU_REC709_EXT                        0x3280
#define EGL_ITU_REC2020_EXT                       0x3281
#define EGL_YUV_FULL_RANGE_EXT                    0x3282
#define EGL_YUV_NARROW_RANGE_EXT                  0x3283
#define EGL_YUV_CHROMA_SITING_0_EXT               0x3284
#define EGL_YUV_CHROMA_SITING_0_5_EXT             0x3285
#endif // EGL_EXT_image_dma_buf_import

#ifndef EGL_EXT_image_dma_buf_import_modifiers
#define EGL_DMA_BUF_PLANE3_FD_EXT                 0x3440
#define EGL_DMA_BUF_PLANE3_OFFSET_EXT             0x3441
#define EGL_DMA_BUF_PLANE3_PITCH_EXT              0x3442
#define EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT        0x3443
#define EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT        0x3444
#define EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT        0x3445
#define EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT        0x3446
#define EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT        0x3447
#define EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT        0x3448
#define EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT        0x3449
#define EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT        0x344A
#endif // EGL_EXT_image_dma_buf_import_modifiers

#endif // KWINEGLEXT_H
