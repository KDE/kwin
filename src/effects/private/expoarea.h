/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwineffects.h>

namespace KWin
{

class ExpoArea : public QObject
{
    Q_OBJECT
    Q_PROPERTY(KWin::EffectScreen *screen READ screen WRITE setScreen NOTIFY screenChanged)
    Q_PROPERTY(qreal x READ x NOTIFY xChanged)
    Q_PROPERTY(qreal y READ y NOTIFY yChanged)
    Q_PROPERTY(qreal width READ width NOTIFY widthChanged)
    Q_PROPERTY(qreal height READ height NOTIFY heightChanged)

public:
    explicit ExpoArea(QObject *parent = nullptr);

    EffectScreen *screen() const;
    void setScreen(EffectScreen *screen);

    qreal x() const;
    qreal y() const;
    qreal width() const;
    qreal height() const;

Q_SIGNALS:
    void screenChanged();
    void xChanged();
    void yChanged();
    void widthChanged();
    void heightChanged();

private:
    void update();

    QRectF m_rect;
    EffectScreen *m_screen = nullptr;
};

} // namespace KWin
