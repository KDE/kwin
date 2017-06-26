/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

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
#include "../abstract_opengl_context_attribute_builder.h"
#include "../egl_context_attribute_builder.h"
#include <QtTest/QtTest>
#include <epoxy/egl.h>

using namespace KWin;

class OpenGLContextAttributeBuilderTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testCtor();
    void testRobust();
    void testVersionMajor();
    void testVersionMajorAndMinor();
    void testEgl_data();
    void testEgl();
    void testGles_data();
    void testGles();
};

class MockOpenGLContextAttributeBuilder : public AbstractOpenGLContextAttributeBuilder
{
public:
    std::vector<int> build() const override;
};

std::vector<int> MockOpenGLContextAttributeBuilder::build() const
{
    return std::vector<int>();
}

void OpenGLContextAttributeBuilderTest::testCtor()
{
    MockOpenGLContextAttributeBuilder builder;
    QCOMPARE(builder.isVersionRequested(), false);
    QCOMPARE(builder.majorVersion(), 0);
    QCOMPARE(builder.minorVersion(), 0);
    QCOMPARE(builder.isRobust(), false);
}

void OpenGLContextAttributeBuilderTest::testRobust()
{
    MockOpenGLContextAttributeBuilder builder;
    QCOMPARE(builder.isRobust(), false);
    builder.setRobust(true);
    QCOMPARE(builder.isRobust(), true);
    builder.setRobust(false);
    QCOMPARE(builder.isRobust(), false);
}

void OpenGLContextAttributeBuilderTest::testVersionMajor()
{
    MockOpenGLContextAttributeBuilder builder;
    builder.setVersion(2);
    QCOMPARE(builder.isVersionRequested(), true);
    QCOMPARE(builder.majorVersion(), 2);
    QCOMPARE(builder.minorVersion(), 0);
    builder.setVersion(3);
    QCOMPARE(builder.isVersionRequested(), true);
    QCOMPARE(builder.majorVersion(), 3);
    QCOMPARE(builder.minorVersion(), 0);
}

void OpenGLContextAttributeBuilderTest::testVersionMajorAndMinor()
{
    MockOpenGLContextAttributeBuilder builder;
    builder.setVersion(2, 1);
    QCOMPARE(builder.isVersionRequested(), true);
    QCOMPARE(builder.majorVersion(), 2);
    QCOMPARE(builder.minorVersion(), 1);
    builder.setVersion(3, 2);
    QCOMPARE(builder.isVersionRequested(), true);
    QCOMPARE(builder.majorVersion(), 3);
    QCOMPARE(builder.minorVersion(), 2);
}

void OpenGLContextAttributeBuilderTest::testEgl_data()
{
    QTest::addColumn<bool>("requestVersion");
    QTest::addColumn<int>("major");
    QTest::addColumn<int>("minor");
    QTest::addColumn<bool>("robust");
    QTest::addColumn<std::vector<int>>("expectedAttribs");

    QTest::newRow("fallback") << false << 0 << 0 << false << std::vector<int>{EGL_NONE};
    QTest::newRow("legacy/robust") << false << 0 << 0 << true <<
        std::vector<int>{
            EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, EGL_LOSE_CONTEXT_ON_RESET_KHR,
            EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR,
            EGL_NONE};
    QTest::newRow("core") << true << 3 << 1 << false <<
        std::vector<int>{
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 1,
            EGL_NONE};
    QTest::newRow("core/robust") << true << 3 << 1 << true <<
        std::vector<int>{
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 1,
            EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, EGL_LOSE_CONTEXT_ON_RESET_KHR,
            EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR,
            EGL_NONE};
}

void OpenGLContextAttributeBuilderTest::testEgl()
{
    QFETCH(bool, requestVersion);
    QFETCH(int, major);
    QFETCH(int, minor);
    QFETCH(bool, robust);

    EglContextAttributeBuilder builder;
    if (requestVersion) {
        builder.setVersion(major, minor);
    }
    builder.setRobust(robust);

    auto attribs = builder.build();
    QTEST(attribs, "expectedAttribs");
}

void OpenGLContextAttributeBuilderTest::testGles_data()
{
    QTest::addColumn<bool>("robust");
    QTest::addColumn<std::vector<int>>("expectedAttribs");

    QTest::newRow("robust") << true << std::vector<int>{
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT,               EGL_TRUE,
        EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT, EGL_LOSE_CONTEXT_ON_RESET_EXT,
        EGL_NONE};
    QTest::newRow("normal") << false << std::vector<int>{
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE};
}

void OpenGLContextAttributeBuilderTest::testGles()
{
    QFETCH(bool, robust);

    EglOpenGLESContextAttributeBuilder builder;
    builder.setVersion(2);
    builder.setRobust(robust);

    auto attribs = builder.build();
    QTEST(attribs, "expectedAttribs");
}

QTEST_GUILESS_MAIN(OpenGLContextAttributeBuilderTest)
#include "opengl_context_attribute_builder_test.moc"
