/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "cursordelegate_vulkan.h"
#include "core/pixelgrid.h"
#include "core/renderlayer.h"
#include "core/rendertarget.h"
#include "vulkan/vulkan_include.h"

namespace KWin
{

CursorDelegateVulkan::CursorDelegateVulkan(Scene *scene, Output *output)
    : SceneDelegate(scene, nullptr)
    , m_output(output)
{
}

CursorDelegateVulkan::~CursorDelegateVulkan()
{
}

void CursorDelegateVulkan::paint(const RenderTarget &renderTarget, const QRegion &region)
{
    // FIXME this doesn't work, because the render pass has been ended in the ItemRenderer already.
    // As a quick fix, start and end the command buffers in the output layer, and start a second render pass with the same cmdbuf here.
    // That's horrible for performance though, so you should think of something better!

    // const QRect cursorRect = snapToPixelGrid(scaledRect(layer()->mapToGlobal(layer()->rect()), m_output->scale()));
    // std::array rects = {
    //     vk::ClearRect(vk::Rect2D(
    //                       vk::Offset2D(cursorRect.x(), cursorRect.y()),
    //                       vk::Extent2D(cursorRect.width(), cursorRect.height())),
    //                   0, 1)};
    // renderTarget.commandBuffer().clearAttachments(vk::ClearAttachment(
    //                                                   vk::ImageAspectFlagBits::eColor,
    //                                                   0,
    //                                                   vk::ClearColorValue(1.0f, 1.0f, 1.0f, 1.0f)),
    //                                               rects);
}
}
