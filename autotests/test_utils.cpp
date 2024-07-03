/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 MBition GmbH
    SPDX-FileContributor: Kai Uwe Broulik <kai_uwe.broulik@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kwin.h>

#include <sys/mman.h>
#include <unistd.h>

#include "utils/cubic_curve.h"
#include "utils/ramfile.h"

#include <QTest>

using namespace KWin;

class TestUtils : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testRamFile();
    void testSealedRamFile();
    void testCubicCurveConstruction();
    void testCubicCurveFunctions();
};

static const QByteArray s_testByteArray = QByteArrayLiteral("Test Data \0\1\2\3");
static const char s_writeTestArray[] = "test";

void TestUtils::testRamFile()
{
    KWin::RamFile file("test", s_testByteArray.constData(), s_testByteArray.size());
    QVERIFY(file.isValid());
    QCOMPARE(file.size(), s_testByteArray.size());

    QVERIFY(file.fd() != -1);

    char buf[20];
    int num = read(file.fd(), buf, sizeof buf);
    QCOMPARE(num, file.size());

    QCOMPARE(qstrcmp(s_testByteArray.constData(), buf), 0);
}

void TestUtils::testSealedRamFile()
{
#if HAVE_MEMFD
    KWin::RamFile file("test", s_testByteArray.constData(), s_testByteArray.size(), KWin::RamFile::Flag::SealWrite);
    QVERIFY(file.isValid());
    QVERIFY(file.effectiveFlags().testFlag(KWin::RamFile::Flag::SealWrite));

    // Writing should not work.
    auto written = write(file.fd(), s_writeTestArray, strlen(s_writeTestArray));
    QCOMPARE(written, -1);

    // Cannot use MAP_SHARED on sealed file descriptor.
    void *data = mmap(nullptr, file.size(), PROT_WRITE, MAP_SHARED, file.fd(), 0);
    QCOMPARE(data, MAP_FAILED);

    data = mmap(nullptr, file.size(), PROT_WRITE, MAP_PRIVATE, file.fd(), 0);
    QVERIFY(data != MAP_FAILED);
#else
    QSKIP("Sealing requires memfd suport.");
#endif
}

void TestUtils::testCubicCurveConstruction()
{
    // Set up a simple curve
    CubicCurve curve;

    // Ensure that the string representation is sound
    QCOMPARE(curve.toString(), "0,0;1,1;");

    CubicCurve deserialized;
    deserialized.fromString(curve.toString());

    // Make sure the serialized string recreates the exact same curve
    QCOMPARE(curve, deserialized);

    // Test copy
    auto copiedCurve = curve;
    QCOMPARE(copiedCurve, curve);
}

void TestUtils::testCubicCurveFunctions()
{
    // Set up a simple curve, which is a straight line
    // X and Y should be 1:1
    {
        CubicCurve curve;
        QCOMPARE(curve.value(0.2), 0.2);
        QCOMPARE(curve.value(0.5), 0.5);
        QCOMPARE(curve.value(0.8), 0.8);
    }

    // Set up a more bendy curve
    // This has a higher Y first control point, and a lower Y second control point
    // Example: https://cubic-bezier.com/#0,.50,1,.50
    {
        CubicCurve curve(QPointF(0.0, 0.5), QPointF(1.0, 0.5));
        QCOMPARE(curve.value(0.2), 0.330711111945);
        QCOMPARE(curve.value(0.5), 0.5);
        QCOMPARE(curve.value(0.8), 0.669288888055);
    }
}

QTEST_MAIN(TestUtils)
#include "test_utils.moc"
