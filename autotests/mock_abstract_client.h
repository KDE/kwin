/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_MOCK_ABSTRACT_CLIENT_H
#define KWIN_MOCK_ABSTRACT_CLIENT_H

#include <QObject>
#include <QRect>

namespace KWin
{

class AbstractClient : public QObject
{
    Q_OBJECT
public:
    explicit AbstractClient(QObject *parent);
    ~AbstractClient() override;

    int screen() const;
    bool isOnScreen(int screen) const;
    bool isActive() const;
    bool isFullScreen() const;
    bool isHiddenInternal() const;
    QRect frameGeometry() const;
    bool keepBelow() const;

    void setActive(bool active);
    void setScreen(int screen);
    void setFullScreen(bool set);
    void setHiddenInternal(bool set);
    void setFrameGeometry(const QRect &rect);
    void setKeepBelow(bool);
    bool isResize() const;
    void setResize(bool set);
    virtual void showOnScreenEdge() = 0;

Q_SIGNALS:
    void geometryChanged();
    void keepBelowChanged();

private:
    bool m_active;
    int m_screen;
    bool m_fullscreen;
    bool m_hiddenInternal;
    bool m_keepBelow;
    QRect m_frameGeometry;
    bool m_resize;
};

}

#endif
