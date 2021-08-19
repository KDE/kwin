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
    Q_PROPERTY(int x READ x NOTIFY xChanged)
    Q_PROPERTY(int y READ y NOTIFY yChanged)
    Q_PROPERTY(int width READ width NOTIFY widthChanged)
    Q_PROPERTY(int height READ height NOTIFY heightChanged)

public:
    explicit ExpoArea(QObject *parent = nullptr);

    EffectScreen *screen() const;
    void setScreen(EffectScreen *screen);

    int x() const;
    int y() const;
    int width() const;
    int height() const;

Q_SIGNALS:
    void screenChanged();
    void xChanged();
    void yChanged();
    void widthChanged();
    void heightChanged();

private:
    void update();

    QRect m_rect;
    EffectScreen *m_screen = nullptr;
};

} // namespace KWin
