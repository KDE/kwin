/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include <memory>

#include "renderlayerdelegate.h"

namespace KWin
{

class GLTexture;

class CursorDelegateOpenGL final : public RenderLayerDelegate
{
    Q_OBJECT

public:
    explicit CursorDelegateOpenGL(QObject *parent = nullptr);
    ~CursorDelegateOpenGL() override;

    void paint(RenderTarget *renderTarget, const QRegion &region) override;

private:
    std::unique_ptr<GLTexture> m_cursorTexture;
    bool m_cursorTextureDirty = false;
};

} // namespace KWin
