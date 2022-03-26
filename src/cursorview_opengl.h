/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "cursorview.h"

namespace KWin
{

class GLTexture;

class OpenGLCursorView final : public CursorView
{
    Q_OBJECT

public:
    explicit OpenGLCursorView(QObject *parent = nullptr);
    ~OpenGLCursorView() override;

    void paint(AbstractOutput *output, const QRegion &region) override;

private:
    QScopedPointer<GLTexture> m_cursorTexture;
    bool m_cursorTextureDirty = false;
};

} // namespace KWin
