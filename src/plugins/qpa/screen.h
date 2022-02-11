/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_QPA_SCREEN_H
#define KWIN_QPA_SCREEN_H

#include <qpa/qplatformscreen.h>

#include <QPointer>
#include <QScopedPointer>

namespace KWin
{
class AbstractOutput;

namespace QPA
{
class Integration;
class PlatformCursor;

class Screen : public QObject, public QPlatformScreen
{
    Q_OBJECT

public:
    Screen(AbstractOutput *output, Integration *integration);
    ~Screen() override;

    QString name() const override;
    QRect geometry() const override;
    int depth() const override;
    QImage::Format format() const override;
    QSizeF physicalSize() const override;
    QPlatformCursor *cursor() const override;
    QDpi logicalDpi() const override;
    qreal devicePixelRatio() const override;
    QList<QPlatformScreen *> virtualSiblings() const override;

private Q_SLOTS:
    void handleGeometryChanged();

private:
    QPointer<AbstractOutput> m_output;
    QScopedPointer<PlatformCursor> m_cursor;
    Integration *m_integration;
};

class PlaceholderScreen : public QPlatformPlaceholderScreen
{
public:
    QDpi logicalDpi() const override;
};

}
}

#endif
