/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mock_drm.h"
#include "../../plugins/platforms/drm/drm_object.h"
#include <QtTest>

class MockDrmObject : public KWin::DrmObject
{
public:
    MockDrmObject(uint32_t id, int fd)
        : DrmObject(id, fd)
    {
    }
    ~MockDrmObject() override {}
    bool atomicInit() override;
    bool initProps() override;

    void setProperties(uint32_t count, uint32_t *props, uint64_t *values) {
        m_count = count;
        m_props = props;
        m_values = values;
    }

    QByteArray name(int prop) const {
        auto property = DrmObject::m_props.at(prop);
        if (!property) {
            return QByteArray();
        }
        return property->name();
    }

    uint32_t propertyId(int prop) const {
        auto property = DrmObject::m_props.at(prop);
        if (!property) {
            return 0xFFFFFFFFu;
        }
        return property->propId();
    }

private:
    uint32_t m_count = 0;
    uint32_t *m_props = nullptr;
    uint64_t *m_values = nullptr;
};

bool MockDrmObject::atomicInit()
{
    return initProps();
}

bool MockDrmObject::initProps()
{
    setPropertyNames({"foo", "bar", "baz"});
    drmModeObjectProperties properties{m_count, m_props, m_values};
    for (int i = 0; i < 3; i++) {
        initProp(i, &properties);
    }
    return false;
}

class ObjectTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testId_data();
    void testId();
    void testFd_data();
    void testFd();
    void testOutput();
    void testInitProperties();
};

void ObjectTest::testId_data()
{
    QTest::addColumn<quint32>("id");

    QTest::newRow("0") << 0u;
    QTest::newRow("1") << 1u;
    QTest::newRow("10") << 10u;
    QTest::newRow("uint max") << 0xFFFFFFFFu;
}

void ObjectTest::testId()
{
    QFETCH(quint32, id);
    MockDrmObject object{id, -1};
    QCOMPARE(object.id(), id);
}

void ObjectTest::testFd_data()
{
    QTest::addColumn<int>("fd");

    QTest::newRow("-1") << -1;
    QTest::newRow("0") << 0;
    QTest::newRow("1") << 1;
    QTest::newRow("2") << 2;
    QTest::newRow("100") << 100;
    QTest::newRow("int max") << 0x7FFFFFFF;
}

void ObjectTest::testFd()
{
    QFETCH(int, fd);
    MockDrmObject object{0, fd};
    QCOMPARE(object.fd(), fd);
}

namespace KWin
{
class DrmOutput {
public:
    int foo;
};
}

void ObjectTest::testOutput()
{
    MockDrmObject object{0, 1};

    QVERIFY(!object.output());

    KWin::DrmOutput output{2};
    object.setOutput(&output);
    QCOMPARE(object.output(), &output);
    QCOMPARE(object.output()->foo, 2);
}

void ObjectTest::testInitProperties()
{
    MockDrmObject object{0, 20};
    uint32_t propertiesIds[] = { 0, 1, 2, 3};
    uint64_t values[] = { 0, 2, 10, 20 };
    object.setProperties(4, propertiesIds, values);

    MockDrm::addDrmModeProperties(20, QVector<_drmModeProperty>{
        _drmModeProperty{
            0,
            0,
            "foo\0",
            0,
            nullptr,
            0,
            nullptr,
            0,
            nullptr
        },
        _drmModeProperty{
            1,
            0,
            "foobar\0",
            0,
            nullptr,
            0,
            nullptr,
            0,
            nullptr
        },
        _drmModeProperty{
            2,
            0,
            "baz\0",
            0,
            nullptr,
            0,
            nullptr,
            0,
            nullptr
        },
        _drmModeProperty{
            3,
            0,
            "foobarbaz\0",
            0,
            nullptr,
            0,
            nullptr,
            0,
            nullptr
        }
    });

    object.atomicInit();

    // verify the names
    QCOMPARE(object.name(0), QByteArrayLiteral("foo"));
    QCOMPARE(object.name(1), QByteArray());
    QCOMPARE(object.name(2), QByteArrayLiteral("baz"));

    // verify the property ids
    QCOMPARE(object.propertyId(0), 0u);
    QCOMPARE(object.propertyId(1), 0xFFFFFFFFu);
    QCOMPARE(object.propertyId(2), 2u);

    // doesn't have enums
    QCOMPARE(object.propHasEnum(0, 0), false);
    QCOMPARE(object.propHasEnum(1, 0), false);
    QCOMPARE(object.propHasEnum(2, 0), false);
}

QTEST_GUILESS_MAIN(ObjectTest)
#include "objecttest.moc"
