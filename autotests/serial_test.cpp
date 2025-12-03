/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>

#include "utils/serial.h"

using namespace KWin;

class TestSerial : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void compare_data();
    void compare();
};

void TestSerial::compare_data()
{
    QTest::addColumn<UInt32Serial>("left");
    QTest::addColumn<UInt32Serial>("right");
    QTest::addColumn<std::weak_ordering>("ordering");

    QTest::addRow("0 <=> 0") << UInt32Serial(0) << UInt32Serial(0) << std::weak_ordering::equivalent;
    QTest::addRow("UINT32_MAX <=> UINT32_MAX") << UInt32Serial(UINT32_MAX) << UInt32Serial(UINT32_MAX) << std::weak_ordering::equivalent;

    QTest::addRow("0 <=> UINT32_MAX") << UInt32Serial(0) << UInt32Serial(UINT32_MAX) << std::weak_ordering::greater;
    QTest::addRow("UINT32_MAX <=> 0") << UInt32Serial(UINT32_MAX) << UInt32Serial(0) << std::weak_ordering::less;

    QTest::addRow("0 <=> UINT32_MAX / 2 - 3") << UInt32Serial(0) << UInt32Serial(UINT32_MAX / 2 - 3) << std::weak_ordering::less;
    QTest::addRow("0 <=> UINT32_MAX / 2 - 2") << UInt32Serial(0) << UInt32Serial(UINT32_MAX / 2 - 2) << std::weak_ordering::less;
    QTest::addRow("0 <=> UINT32_MAX / 2 - 1") << UInt32Serial(0) << UInt32Serial(UINT32_MAX / 2 - 1) << std::weak_ordering::less;
    QTest::addRow("0 <=> UINT32_MAX / 2") << UInt32Serial(0) << UInt32Serial(UINT32_MAX / 2) << std::weak_ordering::less;
    QTest::addRow("0 <=> UINT32_MAX / 2 + 1") << UInt32Serial(0) << UInt32Serial(UINT32_MAX / 2 + 1) << std::weak_ordering::less;
    QTest::addRow("0 <=> UINT32_MAX / 2 + 2") << UInt32Serial(0) << UInt32Serial(UINT32_MAX / 2 + 2) << std::weak_ordering::less;
    QTest::addRow("0 <=> UINT32_MAX / 2 + 3") << UInt32Serial(0) << UInt32Serial(UINT32_MAX / 2 + 3) << std::weak_ordering::greater;

    QTest::addRow("UINT32_MAX / 2 - 2 <=> UINT32_MAX") << UInt32Serial(UINT32_MAX / 2 - 2) << UInt32Serial(UINT32_MAX) << std::weak_ordering::greater;
    QTest::addRow("UINT32_MAX / 2 - 1 <=> UINT32_MAX") << UInt32Serial(UINT32_MAX / 2 - 1) << UInt32Serial(UINT32_MAX) << std::weak_ordering::less;
    QTest::addRow("UINT32_MAX / 2 <=> UINT32_MAX") << UInt32Serial(UINT32_MAX / 2) << UInt32Serial(UINT32_MAX) << std::weak_ordering::less;
    QTest::addRow("UINT32_MAX / 2 + 1 <=> UINT32_MAX") << UInt32Serial(UINT32_MAX / 2 + 1) << UInt32Serial(UINT32_MAX) << std::weak_ordering::less;
    QTest::addRow("UINT32_MAX / 2 + 2 <=> UINT32_MAX") << UInt32Serial(UINT32_MAX / 2 + 2) << UInt32Serial(UINT32_MAX) << std::weak_ordering::less;

    QTest::addRow("UINT32_MAX <=> UINT32_MAX / 2 - 2") << UInt32Serial(UINT32_MAX) << UInt32Serial(UINT32_MAX / 2 - 2) << std::weak_ordering::less;
    QTest::addRow("UINT32_MAX <=> UINT32_MAX / 2 - 1") << UInt32Serial(UINT32_MAX) << UInt32Serial(UINT32_MAX / 2 - 1) << std::weak_ordering::less;
    QTest::addRow("UINT32_MAX <=> UINT32_MAX / 2") << UInt32Serial(UINT32_MAX) << UInt32Serial(UINT32_MAX / 2) << std::weak_ordering::less;
    QTest::addRow("UINT32_MAX <=> UINT32_MAX / 2 + 1") << UInt32Serial(UINT32_MAX) << UInt32Serial(UINT32_MAX / 2 + 1) << std::weak_ordering::less;
    QTest::addRow("UINT32_MAX <=> UINT32_MAX / 2 + 2") << UInt32Serial(UINT32_MAX) << UInt32Serial(UINT32_MAX / 2 + 2) << std::weak_ordering::greater;

    QTest::addRow("UINT32_MAX / 2 - 1 <=> 0") << UInt32Serial(UINT32_MAX / 2 - 1) << UInt32Serial(0) << std::weak_ordering::greater;
    QTest::addRow("UINT32_MAX / 2 <=> 0") << UInt32Serial(UINT32_MAX / 2) << UInt32Serial(0) << std::weak_ordering::less;
    QTest::addRow("UINT32_MAX / 2 + 1 <=> 0") << UInt32Serial(UINT32_MAX / 2 + 1) << UInt32Serial(0) << std::weak_ordering::less;
}

void TestSerial::compare()
{
    QFETCH(UInt32Serial, left);
    QFETCH(UInt32Serial, right);
    QFETCH(std::weak_ordering, ordering);

    QCOMPARE(left <=> right, ordering);
}

QTEST_MAIN(TestSerial)

#include "serial_test.moc"
