/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "renderlayerdelegate.h"

namespace KWin
{

class AbstractOutput;

class KWIN_EXPORT CursorView : public QObject
{
    Q_OBJECT

public:
    explicit CursorView(QObject *parent = nullptr);

    virtual void paint(AbstractOutput *output, const QRegion &region) = 0;
};

class KWIN_EXPORT CursorDelegate : public RenderLayerDelegate
{
    Q_OBJECT

public:
    CursorDelegate(AbstractOutput *output, CursorView *view);

    void paint(const QRegion &damage, const QRegion &repaint, QRegion &update, QRegion &valid) override;

private:
    CursorView *m_view;
    AbstractOutput *m_output;
};

} // namespace KWin
