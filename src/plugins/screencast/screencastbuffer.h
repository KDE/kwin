/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/graphicsbufferview.h"
#include "core/syncobjtimeline.h"

#include <pipewire/pipewire.h>

namespace KWin
{

class GLFramebuffer;
class GLTexture;
class GraphicsBuffer;
struct GraphicsBufferOptions;
class RenderDevice;

class ScreenCastBuffer
{
public:
    explicit ScreenCastBuffer(GraphicsBuffer *buffer);
    virtual ~ScreenCastBuffer();

    int m_age = 0;
    GraphicsBuffer *const m_buffer;
};

class DmaBufScreenCastBuffer : public ScreenCastBuffer
{
public:
    static DmaBufScreenCastBuffer *create(RenderDevice *renderDevice, pw_buffer *pwBuffer, const GraphicsBufferOptions &options);

    explicit DmaBufScreenCastBuffer(GraphicsBuffer *buffer, std::unique_ptr<SyncTimeline> &&synctimeline);

    std::unique_ptr<SyncTimeline> synctimeline;
};

class MemFdScreenCastBuffer : public ScreenCastBuffer
{
public:
    static MemFdScreenCastBuffer *create(pw_buffer *pwBuffer, const GraphicsBufferOptions &options);

    explicit MemFdScreenCastBuffer(GraphicsBuffer *buffer, GraphicsBufferView &&view);

    GraphicsBufferView view;
};

} // namespace KWin
