/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_wayland_test.h"

#include <QObject>

class GenericSceneOpenGLTest : public QObject
{
Q_OBJECT
public:
    ~GenericSceneOpenGLTest() override;
protected:
    GenericSceneOpenGLTest(const QByteArray &envVariable);
private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void testRestart_data();
    void testRestart();

private:
    QByteArray m_envVariable;
};
