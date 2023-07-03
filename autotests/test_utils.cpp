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

#include "utils/ramfile.h"

#include <QTest>

using namespace KWin;

class TestUtils : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testRamFile();
    void testSealedRamFile();
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

QTEST_MAIN(TestUtils)
#include "test_utils.moc"
