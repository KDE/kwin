/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <qpa/qplatformcursor.h>

namespace KWin
{
namespace QPA
{

class PlatformCursor : public QPlatformCursor
{
public:
    PlatformCursor();
    ~PlatformCursor() override;
    QPoint pos() const override;
    void setPos(const QPoint &pos) override;
    void changeCursor(QCursor *windowCursor, QWindow *window) override;
};

}
}
