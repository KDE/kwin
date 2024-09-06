/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/scene.h"

namespace KWin
{

class GLFramebuffer;
class GLTexture;
class Output;

class CursorDelegateOpenGL final : public QObject, public SceneDelegate
{
    Q_OBJECT

public:
    CursorDelegateOpenGL(Scene *scene, Output *output);
    ~CursorDelegateOpenGL() override;

    void paint(const RenderTarget &renderTarget, const QRegion &region) override;

private:
    Output *const m_output;
    std::unique_ptr<GLTexture> m_texture;
    std::unique_ptr<GLFramebuffer> m_framebuffer;
};

} // namespace KWin
