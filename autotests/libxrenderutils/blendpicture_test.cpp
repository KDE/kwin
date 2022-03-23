/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QLoggingCategory>
#include <QtTest>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <private/qtx11extras_p.h>
#else
#include <QX11Info>
#endif

#include "../testutils.h"

#include "kwinxrenderutils.h"

class BlendPictureTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void testDontCrashOnTeardown();
};

void BlendPictureTest::initTestCase()
{
    KWin::XRenderUtils::init(QX11Info::connection(), QX11Info::appRootWindow());
}

void BlendPictureTest::cleanupTestCase()
{
    KWin::XRenderUtils::cleanup();
}

void BlendPictureTest::testDontCrashOnTeardown()
{
    // this test uses xrenderBlendPicture - the only idea is to trigger the creation
    // closing the application should not crash
    // see BUG 363251
    const auto picture = KWin::xRenderBlendPicture(0.5);
    // and a second one
    const auto picture2 = KWin::xRenderBlendPicture(0.6);
    Q_UNUSED(picture)
    Q_UNUSED(picture2)
}

Q_CONSTRUCTOR_FUNCTION(forceXcb)
QTEST_MAIN(BlendPictureTest)
#include "blendpicture_test.moc"
