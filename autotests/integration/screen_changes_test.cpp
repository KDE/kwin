/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "pointer_input.h"
#include "wayland_server.h"
#include "workspace.h"

#include "qwayland-xdg-output-unstable-v1.h"

#include <KWayland/Client/registry.h>

using namespace KWin;

class WlOutput : public QObject, public QtWayland::wl_output
{
    Q_OBJECT

public:
    WlOutput(::wl_registry *registry, uint32_t id, int version)
        : QtWayland::wl_output(registry, id, version)
    {
    }

    ~WlOutput() override
    {
        release();
    }

    QPoint globalPosition() const
    {
        return m_globalPosition;
    }

    QSize pixelSize() const
    {
        return m_pixelSize;
    }

    QString name() const
    {
        return m_name;
    }

Q_SIGNALS:
    void changed();

private:
    void output_geometry(int32_t x, int32_t y, int32_t, int32_t, int32_t, const QString &, const QString &, int32_t) override
    {
        m_globalPosition = QPoint(x, y);
    }

    void output_mode(uint32_t flags, int32_t width, int32_t height, int32_t) override
    {
        if (flags & mode_current) {
            m_pixelSize = QSize(width, height);
        }
    }

    void output_done() override
    {
        Q_EMIT changed();
    }

    void output_name(const QString &name) override
    {
        m_name = name;
    }

    QPoint m_globalPosition;
    QSize m_pixelSize;
    QString m_name;
};

class XdgOutput : public QObject, public QtWayland::zxdg_output_v1
{
    Q_OBJECT

public:
    explicit XdgOutput(::zxdg_output_v1 *object)
        : QtWayland::zxdg_output_v1(object)
    {
    }

    ~XdgOutput() override
    {
        destroy();
    }

    QPoint logicalPosition() const
    {
        return m_logicalPosition;
    }

    QSize logicalSize() const
    {
        return m_logicalSize;
    }

    QString name() const
    {
        return m_name;
    }

    QString description() const
    {
        return m_description;
    }

Q_SIGNALS:
    void changed();

private:
    void zxdg_output_v1_logical_position(int32_t x, int32_t y) override
    {
        m_logicalPosition = QPoint(x, y);
    }

    void zxdg_output_v1_logical_size(int32_t width, int32_t height) override
    {
        m_logicalSize = QSize(width, height);
        Q_EMIT changed();
    }

    void zxdg_output_v1_done() override
    {
        Q_EMIT changed();
    }

    void zxdg_output_v1_name(const QString &name) override
    {
        m_name = name;
    }

    void zxdg_output_v1_description(const QString &description) override
    {
        m_description = description;
    }

    QPoint m_logicalPosition;
    QSize m_logicalSize;
    QString m_name;
    QString m_description;
};

class ScreenChangesTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testScreenAddRemove();
};

void ScreenChangesTest::initTestCase()
{
    QVERIFY(waylandServer()->init(qAppName()));

    kwinApp()->start();
    Test::setOutputConfig({Rect(0, 0, 1280, 1024)});
    setenv("QT_QPA_PLATFORM", "wayland", true);
}

void ScreenChangesTest::init()
{
    QVERIFY(Test::setupWaylandConnection());

    workspace()->setActiveOutput(QPoint(640, 512));
    KWin::input()->pointer()->warp(QPoint(640, 512));
}

void ScreenChangesTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void ScreenChangesTest::testScreenAddRemove()
{
    // this test verifies that when a new screen is added it gets synced to Wayland

    // first create a registry to get signals about Outputs announced/removed
    KWayland::Client::Registry registry;
    QSignalSpy allAnnounced(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    QSignalSpy outputAnnouncedSpy(&registry, &KWayland::Client::Registry::outputAnnounced);
    QSignalSpy outputRemovedSpy(&registry, &KWayland::Client::Registry::outputRemoved);
    registry.create(Test::waylandConnection());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(allAnnounced.wait());
    const auto xdgOMData = registry.interface(KWayland::Client::Registry::Interface::XdgOutputUnstableV1);
    QtWayland::zxdg_output_manager_v1 xdgOutputManager(registry.registry(), xdgOMData.name, xdgOMData.version);

    // should be one output
    QCOMPARE(workspace()->outputs().count(), 1);
    QCOMPARE(outputAnnouncedSpy.count(), 1);
    const quint32 firstOutputId = outputAnnouncedSpy.first().first().value<quint32>();
    QVERIFY(firstOutputId != 0u);
    outputAnnouncedSpy.clear();

    // let's announce a new output
    const QList<Rect> geometries{Rect(0, 0, 1280, 1024), Rect(1280, 0, 1280, 1024)};
    Test::setOutputConfig(geometries);
    auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), geometries[0]);
    QCOMPARE(outputs[1]->geometry(), geometries[1]);

    // this should result in it getting announced, two new outputs are added...
    QVERIFY(outputAnnouncedSpy.wait());
    if (outputAnnouncedSpy.count() < 2) {
        QVERIFY(outputAnnouncedSpy.wait());
    }
    QCOMPARE(outputAnnouncedSpy.count(), 2);
    // ... and afterward the previous output gets removed
    if (outputRemovedSpy.isEmpty()) {
        QVERIFY(outputRemovedSpy.wait());
    }
    QCOMPARE(outputRemovedSpy.count(), 1);
    QCOMPARE(outputRemovedSpy.first().first().value<quint32>(), firstOutputId);

    // let's wait a little bit to ensure we don't get more events
    QTest::qWait(100);
    QCOMPARE(outputAnnouncedSpy.count(), 2);
    QCOMPARE(outputRemovedSpy.count(), 1);

    // let's create the output objects to ensure they are correct
    WlOutput o1(registry.registry(), outputAnnouncedSpy.first().first().value<quint32>(), outputAnnouncedSpy.first().last().value<quint32>());
    QSignalSpy o1ChangedSpy(&o1, &WlOutput::changed);
    QVERIFY(o1ChangedSpy.wait());
    KWin::LogicalOutput *serverOutput1 = workspace()->findOutput(o1.name()); // use wl_output.name to find the compositor side output
    QCOMPARE(o1.globalPosition(), serverOutput1->geometry().topLeft());
    QCOMPARE(o1.pixelSize(), serverOutput1->modeSize());
    WlOutput o2(registry.registry(), outputAnnouncedSpy.last().first().value<quint32>(), outputAnnouncedSpy.last().last().value<quint32>());
    QSignalSpy o2ChangedSpy(&o2, &WlOutput::changed);
    QVERIFY(o2ChangedSpy.wait());
    KWin::LogicalOutput *serverOutput2 = workspace()->findOutput(o2.name()); // use wl_output.name to find the compositor side output
    QCOMPARE(o2.globalPosition(), serverOutput2->geometry().topLeft());
    QCOMPARE(o2.pixelSize(), serverOutput2->modeSize());

    // and check XDGOutput is synced
    XdgOutput xdgO1(xdgOutputManager.get_xdg_output(o1.object()));
    QSignalSpy xdgO1ChangedSpy(&xdgO1, &XdgOutput::changed);
    QVERIFY(xdgO1ChangedSpy.wait());
    QCOMPARE(xdgO1.logicalPosition(), serverOutput1->geometry().topLeft());
    QCOMPARE(xdgO1.logicalSize(), serverOutput1->geometry().size());
    XdgOutput xdgO2(xdgOutputManager.get_xdg_output(o2.object()));
    QSignalSpy xdgO2ChangedSpy(&xdgO2, &XdgOutput::changed);
    QVERIFY(xdgO2ChangedSpy.wait());
    QCOMPARE(xdgO2.logicalPosition(), serverOutput2->geometry().topLeft());
    QCOMPARE(xdgO2.logicalSize(), serverOutput2->geometry().size());

    QVERIFY(xdgO1.name().startsWith("Virtual-"));
    QVERIFY(xdgO1.name() != xdgO2.name());
    QVERIFY(!xdgO1.description().isEmpty());

    // now let's try to remove one output again
    outputAnnouncedSpy.clear();
    outputRemovedSpy.clear();

    const QList<Rect> geometries2{Rect(0, 0, 1280, 1024)};
    Test::setOutputConfig(geometries2);
    outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 1);
    QCOMPARE(outputs[0]->geometry(), geometries2.at(0));

    QVERIFY(outputAnnouncedSpy.wait());
    QCOMPARE(outputAnnouncedSpy.count(), 1);
    QVERIFY(outputRemovedSpy.count() == 2 || outputRemovedSpy.wait());
    // now wait a bit to ensure we don't get more events
    QTest::qWait(100);
    QCOMPARE(outputAnnouncedSpy.count(), 1);
    QCOMPARE(outputRemovedSpy.count(), 2);
}

WAYLANDTEST_MAIN(ScreenChangesTest)
#include "screen_changes_test.moc"
