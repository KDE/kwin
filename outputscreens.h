/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_OUTPUTSCREENS_H
#define KWIN_OUTPUTSCREENS_H

#include "screens.h"

namespace KWin
{

class AbstractOutput;

/**
 * @brief Implementation for backends with Outputs
 */
class KWIN_EXPORT OutputScreens : public Screens
{
    Q_OBJECT
public:
    OutputScreens(Platform *platform, QObject *parent = nullptr);
    ~OutputScreens() override;

    void init() override;
    QString name(int screen) const override;
    bool isInternal(int screen) const override;
    QSizeF physicalSize(int screen) const override;
    QRect geometry(int screen) const override;
    QSize size(int screen) const override;
    qreal scale(int screen) const override;
    float refreshRate(int screen) const override;
    void updateCount() override;
    int number(const QPoint &pos) const override;

protected:
    Platform *m_platform;

private:
    AbstractOutput *findOutput(int screen) const;
};

}

#endif // KWIN_OUTPUTSCREENS_H
