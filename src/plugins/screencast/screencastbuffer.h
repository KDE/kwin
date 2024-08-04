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

class ScreenCastBuffer
{
public:
    explicit ScreenCastBuffer(GraphicsBuffer *buffer);
    virtual ~ScreenCastBuffer();

private:
    GraphicsBuffer *m_buffer;
};

class DmaBufScreenCastBuffer : public ScreenCastBuffer
{
public:
    static DmaBufScreenCastBuffer *create(pw_buffer *pwBuffer, const GraphicsBufferOptions &options);

    std::shared_ptr<GLTexture> texture;
    std::unique_ptr<GLFramebuffer> framebuffer;
    std::unique_ptr<SyncTimeline> synctimeline;

private:
    DmaBufScreenCastBuffer(GraphicsBuffer *buffer, std::shared_ptr<GLTexture> &&texture, std::unique_ptr<GLFramebuffer> &&framebuffer, std::unique_ptr<SyncTimeline> &&synctimeline);
};

class MemFdScreenCastBuffer : public ScreenCastBuffer
{
public:
    static MemFdScreenCastBuffer *create(pw_buffer *pwBuffer, const GraphicsBufferOptions &options);

    GraphicsBufferView view;

private:
    MemFdScreenCastBuffer(GraphicsBuffer *buffer, GraphicsBufferView &&view);
};

} // namespace KWin
