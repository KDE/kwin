/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include <QtTest>
#include <QLoggingCategory>
#include <QX11Info>

#include "../testutils.h"
#include "../../libkwineffects/kwinxrenderutils.h"

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
