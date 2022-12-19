/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include <QObject>
#include <memory>

#include "core/renderlayerdelegate.h"

namespace KWin
{

class GLFramebuffer;
class GLTexture;

class CursorDelegateOpenGL final : public QObject, public RenderLayerDelegate
{
    Q_OBJECT

public:
    ~CursorDelegateOpenGL() override;

    void paint(RenderTarget *renderTarget, const QRegion &region) override;

private:
    std::unique_ptr<GLTexture> m_texture;
    std::unique_ptr<GLFramebuffer> m_framebuffer;
};

} // namespace KWin
