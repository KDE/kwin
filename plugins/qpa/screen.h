/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_QPA_SCREEN_H
#define KWIN_QPA_SCREEN_H

#include <qpa/qplatformscreen.h>
#include <QScopedPointer>

namespace KWin
{
namespace QPA
{
class PlatformCursor;

class Screen : public QPlatformScreen
{
public:
    explicit Screen(int screen);
    ~Screen() override;

    QRect geometry() const override;
    int depth() const override;
    QImage::Format format() const override;
    QSizeF physicalSize() const override;
    QPlatformCursor *cursor() const override;
    QDpi logicalDpi() const override;
    qreal devicePixelRatio() const override;

private:
    int m_screen;
    QScopedPointer<PlatformCursor> m_cursor;
};

}
}

#endif
