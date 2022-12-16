/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <qpa/qplatformscreen.h>

#include <QPointer>

namespace KWin
{
class Output;

namespace QPA
{
class Integration;
class PlatformCursor;

class Screen : public QObject, public QPlatformScreen
{
    Q_OBJECT

public:
    Screen(Output *output, Integration *integration);
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
    QPointer<Output> m_output;
    std::unique_ptr<PlatformCursor> m_cursor;
    Integration *m_integration;
};

class PlaceholderScreen : public QPlatformPlaceholderScreen
{
public:
    QDpi logicalDpi() const override;
};

}
}
