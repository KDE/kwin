/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "../plugins/platforms/drm/gbm_surface.h"
#include <QtTest>

#include <gbm.h>

// mocking

struct gbm_device {
    bool surfaceShouldFail = false;
};

struct gbm_surface {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t flags;
};

struct gbm_bo {
};

struct gbm_surface *gbm_surface_create(struct gbm_device *gbm, uint32_t width, uint32_t height, uint32_t format, uint32_t flags)
{
    if (gbm && gbm->surfaceShouldFail) {
        return nullptr;
    }
    auto ret = new gbm_surface{width, height, format, flags};
    return ret;
}

void gbm_surface_destroy(struct gbm_surface *surface)
{
    delete surface;
}

struct gbm_bo *gbm_surface_lock_front_buffer(struct gbm_surface *surface)
{
    Q_UNUSED(surface)
    return new gbm_bo;
}

void gbm_surface_release_buffer(struct gbm_surface *surface, struct gbm_bo *bo)
{
    Q_UNUSED(surface)
    delete bo;
}

using KWin::GbmSurface;

class GbmSurfaceTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testCreate();
    void testCreateFailure();
    void testBo();
};

void GbmSurfaceTest::testCreate()
{
    GbmSurface surface(nullptr, 2, 3, 4, 5);
    gbm_surface *native = surface.surface();
    QVERIFY(surface);
    QCOMPARE(native->width, 2u);
    QCOMPARE(native->height, 3u);
    QCOMPARE(native->format, 4u);
    QCOMPARE(native->flags, 5u);
}

void GbmSurfaceTest::testCreateFailure()
{
    gbm_device dev{true};
    GbmSurface surface(&dev, 2, 3, 4, 5);
    QVERIFY(!surface);
    gbm_surface *native = surface.surface();
    QVERIFY(!native);
}

void GbmSurfaceTest::testBo()
{
    GbmSurface surface(nullptr, 2, 3, 4, 5);
    // release buffer on nullptr should not be a problem
    surface.releaseBuffer(nullptr);
    // now an actual buffer
    auto bo = surface.lockFrontBuffer();
    surface.releaseBuffer(bo);

    // and a surface which fails
    gbm_device dev{true};
    GbmSurface surface2(&dev, 2, 3, 4, 5);
    QVERIFY(!surface2.lockFrontBuffer());
    auto bo2 = surface.lockFrontBuffer();
    // this won't do anything
    surface2.releaseBuffer(bo2);
    // so we need to clean up properly
    surface.releaseBuffer(bo2);
}

QTEST_GUILESS_MAIN(GbmSurfaceTest)
#include "test_gbm_surface.moc"
