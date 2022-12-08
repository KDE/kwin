/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include <memory>

#include "cursordelegate.h"

namespace KWin
{

class GLTexture;
class Output;

class CursorDelegateOpenGL final : public CursorDelegate
{
    Q_OBJECT

public:
    explicit CursorDelegateOpenGL(Output *output, QObject *parent = nullptr);
    ~CursorDelegateOpenGL() override;

    void paint(RenderTarget *renderTarget, const QRegion &region) override;

private:
    std::unique_ptr<GLTexture> m_cursorTexture;
    qint64 m_cacheKey = 0;
};

} // namespace KWin
