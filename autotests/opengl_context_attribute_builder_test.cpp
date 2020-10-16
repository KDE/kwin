/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "../abstract_opengl_context_attribute_builder.h"
#include "../egl_context_attribute_builder.h"
#include <QtTest>
#include <epoxy/egl.h>

#include <kwinconfig.h>
#if HAVE_EPOXY_GLX
#include "../plugins/platforms/x11/standalone/glx_context_attribute_builder.h"
#include <epoxy/glx.h>

#ifndef GLX_GENERATE_RESET_ON_VIDEO_MEMORY_PURGE_NV
#define GLX_GENERATE_RESET_ON_VIDEO_MEMORY_PURGE_NV 0x20F7
#endif
#endif

using namespace KWin;

class OpenGLContextAttributeBuilderTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testCtor();
    void testRobust();
    void testForwardCompatible();
    void testProfile();
    void testResetOnVideoMemoryPurge();
    void testVersionMajor();
    void testVersionMajorAndMinor();
    void testHighPriority();
    void testEgl_data();
    void testEgl();
    void testGles_data();
    void testGles();
    void testGlx_data();
    void testGlx();
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
    QCOMPARE(builder.isForwardCompatible(), false);
    QCOMPARE(builder.isCoreProfile(), false);
    QCOMPARE(builder.isCompatibilityProfile(), false);
    QCOMPARE(builder.isResetOnVideoMemoryPurge(), false);
    QCOMPARE(builder.isHighPriority(), false);
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

void OpenGLContextAttributeBuilderTest::testForwardCompatible()
{
    MockOpenGLContextAttributeBuilder builder;
    QCOMPARE(builder.isForwardCompatible(), false);
    builder.setForwardCompatible(true);
    QCOMPARE(builder.isForwardCompatible(), true);
    builder.setForwardCompatible(false);
    QCOMPARE(builder.isForwardCompatible(), false);
}

void OpenGLContextAttributeBuilderTest::testProfile()
{
    MockOpenGLContextAttributeBuilder builder;
    QCOMPARE(builder.isCoreProfile(), false);
    QCOMPARE(builder.isCompatibilityProfile(), false);
    builder.setCoreProfile(true);
    QCOMPARE(builder.isCoreProfile(), true);
    QCOMPARE(builder.isCompatibilityProfile(), false);
    builder.setCompatibilityProfile(true);
    QCOMPARE(builder.isCoreProfile(), false);
    QCOMPARE(builder.isCompatibilityProfile(), true);
    builder.setCoreProfile(true);
    QCOMPARE(builder.isCoreProfile(), true);
    QCOMPARE(builder.isCompatibilityProfile(), false);
}

void OpenGLContextAttributeBuilderTest::testResetOnVideoMemoryPurge()
{
    MockOpenGLContextAttributeBuilder builder;
    QCOMPARE(builder.isResetOnVideoMemoryPurge(), false);
    builder.setResetOnVideoMemoryPurge(true);
    QCOMPARE(builder.isResetOnVideoMemoryPurge(), true);
    builder.setResetOnVideoMemoryPurge(false);
    QCOMPARE(builder.isResetOnVideoMemoryPurge(), false);
}

void OpenGLContextAttributeBuilderTest::testHighPriority()
{
    MockOpenGLContextAttributeBuilder builder;
    QCOMPARE(builder.isHighPriority(), false);
    builder.setHighPriority(true);
    QCOMPARE(builder.isHighPriority(), true);
    builder.setHighPriority(false);
    QCOMPARE(builder.isHighPriority(), false);
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
    QTest::addColumn<bool>("forwardCompatible");
    QTest::addColumn<bool>("coreProfile");
    QTest::addColumn<bool>("compatibilityProfile");
    QTest::addColumn<bool>("highPriority");
    QTest::addColumn<std::vector<int>>("expectedAttribs");

    QTest::newRow("fallback") << false << 0 << 0 << false << false << false << false << false << std::vector<int>{EGL_NONE};
    QTest::newRow("legacy/robust") << false << 0 << 0 << true << false << false << false << false <<
        std::vector<int>{
            EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, EGL_LOSE_CONTEXT_ON_RESET_KHR,
            EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR,
            EGL_NONE};
    QTest::newRow("legacy/robust/high priority") << false << 0 << 0 << true << false << false << false << true <<
        std::vector<int>{
            EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, EGL_LOSE_CONTEXT_ON_RESET_KHR,
            EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR,
            EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_HIGH_IMG,
            EGL_NONE};
    QTest::newRow("core") << true << 3 << 1 << false << false << false << false << false <<
        std::vector<int>{
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 1,
            EGL_NONE};
    QTest::newRow("core/high priority") << true << 3 << 1 << false << false << false << false << true <<
        std::vector<int>{
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 1,
            EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_HIGH_IMG,
            EGL_NONE};
    QTest::newRow("core/robust") << true << 3 << 1 << true << false << false << false << false <<
        std::vector<int>{
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 1,
            EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, EGL_LOSE_CONTEXT_ON_RESET_KHR,
            EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR,
            EGL_NONE};
    QTest::newRow("core/robust/high priority") << true << 3 << 1 << true << false << false << false << true <<
        std::vector<int>{
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 1,
            EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, EGL_LOSE_CONTEXT_ON_RESET_KHR,
            EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR,
            EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_HIGH_IMG,
            EGL_NONE};
    QTest::newRow("core/robust/forward compatible") << true << 3 << 1 << true << true << false << false << false <<
        std::vector<int>{
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 1,
            EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, EGL_LOSE_CONTEXT_ON_RESET_KHR,
            EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR | EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
            EGL_NONE};
    QTest::newRow("core/robust/forward compatible/high priority") << true << 3 << 1 << true << true << false << false << true <<
        std::vector<int>{
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 1,
            EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, EGL_LOSE_CONTEXT_ON_RESET_KHR,
            EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR | EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
            EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_HIGH_IMG,
            EGL_NONE};
    QTest::newRow("core/forward compatible") << true << 3 << 1 << false << true << false << false << false <<
        std::vector<int>{
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 1,
            EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
            EGL_NONE};
    QTest::newRow("core/forward compatible/high priority") << true << 3 << 1 << false << true << false << false << true <<
        std::vector<int>{
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 1,
            EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
            EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_HIGH_IMG,
            EGL_NONE};
    QTest::newRow("core profile/forward compatible") << true << 3 << 2 << false << true << true << false << false <<
        std::vector<int>{
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 2,
            EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
            EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
            EGL_NONE};
    QTest::newRow("core profile/forward compatible/high priority") << true << 3 << 2 << false << true << true << false << true <<
        std::vector<int>{
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 2,
            EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
            EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
            EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_HIGH_IMG,
            EGL_NONE};
    QTest::newRow("compatibility profile/forward compatible") << true << 3 << 2 << false << true << false << true << false <<
        std::vector<int>{
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 2,
            EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
            EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR,
            EGL_NONE};
    QTest::newRow("compatibility profile/forward compatible/high priority") << true << 3 << 2 << false << true << false << true << true <<
        std::vector<int>{
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 2,
            EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
            EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR,
            EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_HIGH_IMG,
            EGL_NONE};
    QTest::newRow("core profile/robust/forward compatible") << true << 3 << 2 << true << true << true << false << false <<
        std::vector<int>{
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 2,
            EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, EGL_LOSE_CONTEXT_ON_RESET_KHR,
            EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR | EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
            EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
            EGL_NONE};
    QTest::newRow("core profile/robust/forward compatible/high priority") << true << 3 << 2 << true << true << true << false << true <<
        std::vector<int>{
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 2,
            EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, EGL_LOSE_CONTEXT_ON_RESET_KHR,
            EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR | EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
            EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
            EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_HIGH_IMG,
            EGL_NONE};
    QTest::newRow("compatibility profile/robust/forward compatible") << true << 3 << 2 << true << true << false << true << false <<
        std::vector<int>{
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 2,
            EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, EGL_LOSE_CONTEXT_ON_RESET_KHR,
            EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR | EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
            EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR,
            EGL_NONE};
    QTest::newRow("compatibility profile/robust/forward compatible/high priority") << true << 3 << 2 << true << true << false << true << true <<
        std::vector<int>{
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 2,
            EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR, EGL_LOSE_CONTEXT_ON_RESET_KHR,
            EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR | EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
            EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR,
            EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_HIGH_IMG,
            EGL_NONE};
}

void OpenGLContextAttributeBuilderTest::testEgl()
{
    QFETCH(bool, requestVersion);
    QFETCH(int, major);
    QFETCH(int, minor);
    QFETCH(bool, robust);
    QFETCH(bool, forwardCompatible);
    QFETCH(bool, coreProfile);
    QFETCH(bool, compatibilityProfile);
    QFETCH(bool, highPriority);

    EglContextAttributeBuilder builder;
    if (requestVersion) {
        builder.setVersion(major, minor);
    }
    builder.setRobust(robust);
    builder.setForwardCompatible(forwardCompatible);
    builder.setCoreProfile(coreProfile);
    builder.setCompatibilityProfile(compatibilityProfile);
    builder.setHighPriority(highPriority);

    auto attribs = builder.build();
    QTEST(attribs, "expectedAttribs");
}

void OpenGLContextAttributeBuilderTest::testGles_data()
{
    QTest::addColumn<bool>("robust");
    QTest::addColumn<bool>("highPriority");
    QTest::addColumn<std::vector<int>>("expectedAttribs");

    QTest::newRow("robust") << true << false << std::vector<int>{
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT,               EGL_TRUE,
        EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT, EGL_LOSE_CONTEXT_ON_RESET_EXT,
        EGL_NONE};
    QTest::newRow("robust/high priority") << true << true << std::vector<int>{
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT,               EGL_TRUE,
        EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT, EGL_LOSE_CONTEXT_ON_RESET_EXT,
        EGL_CONTEXT_PRIORITY_LEVEL_IMG,                     EGL_CONTEXT_PRIORITY_HIGH_IMG,
        EGL_NONE};
    QTest::newRow("normal") << false << false << std::vector<int>{
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE};
    QTest::newRow("normal/high priority") << false << true << std::vector<int>{
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_HIGH_IMG,
        EGL_NONE};
}

void OpenGLContextAttributeBuilderTest::testGles()
{
    QFETCH(bool, robust);
    QFETCH(bool, highPriority);

    EglOpenGLESContextAttributeBuilder builder;
    builder.setVersion(2);
    builder.setRobust(robust);
    builder.setHighPriority(highPriority);

    auto attribs = builder.build();
    QTEST(attribs, "expectedAttribs");
}

void OpenGLContextAttributeBuilderTest::testGlx_data()
{
#if HAVE_EPOXY_GLX
    QTest::addColumn<bool>("requestVersion");
    QTest::addColumn<int>("major");
    QTest::addColumn<int>("minor");
    QTest::addColumn<bool>("robust");
    QTest::addColumn<bool>("videoPurge");
    QTest::addColumn<std::vector<int>>("expectedAttribs");

    QTest::newRow("fallback") << true << 2 << 1 << false << false << std::vector<int>{
        GLX_CONTEXT_MAJOR_VERSION_ARB, 2,
        GLX_CONTEXT_MINOR_VERSION_ARB, 1,
        0};
    QTest::newRow("legacy/robust") << false << 0 << 0 << true << false << std::vector<int>{
        GLX_CONTEXT_FLAGS_ARB,                       GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB,
        GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, GLX_LOSE_CONTEXT_ON_RESET_ARB,
        0
    };
    QTest::newRow("legacy/robust/videoPurge") << false << 0 << 0 << true << true << std::vector<int>{
        GLX_CONTEXT_FLAGS_ARB,                       GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB,
        GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, GLX_LOSE_CONTEXT_ON_RESET_ARB,
        GLX_GENERATE_RESET_ON_VIDEO_MEMORY_PURGE_NV, GL_TRUE,
        0
    };
    QTest::newRow("core") << true << 3 << 1 << false << false << std::vector<int>{
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 1,
        0};
    QTest::newRow("core/robust") << true << 3 << 1 << true << false << std::vector<int>{
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 1,
        GLX_CONTEXT_FLAGS_ARB,                       GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB,
        GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, GLX_LOSE_CONTEXT_ON_RESET_ARB,
        0
    };
    QTest::newRow("core/robust/videoPurge") << true << 3 << 1 << true << true << std::vector<int>{
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 1,
        GLX_CONTEXT_FLAGS_ARB,                       GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB,
        GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, GLX_LOSE_CONTEXT_ON_RESET_ARB,
        GLX_GENERATE_RESET_ON_VIDEO_MEMORY_PURGE_NV, GL_TRUE,
        0
    };
#endif
}

void OpenGLContextAttributeBuilderTest::testGlx()
{
#if HAVE_EPOXY_GLX
    QFETCH(bool, requestVersion);
    QFETCH(int, major);
    QFETCH(int, minor);
    QFETCH(bool, robust);
    QFETCH(bool, videoPurge);

    GlxContextAttributeBuilder builder;
    if (requestVersion) {
        builder.setVersion(major, minor);
    }
    builder.setRobust(robust);
    builder.setResetOnVideoMemoryPurge(videoPurge);

    auto attribs = builder.build();
    QTEST(attribs, "expectedAttribs");
#endif
}

QTEST_GUILESS_MAIN(OpenGLContextAttributeBuilderTest)
#include "opengl_context_attribute_builder_test.moc"
