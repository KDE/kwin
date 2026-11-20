/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 MBition GmbH
    SPDX-FileContributor: Kai Uwe Broulik <kai_uwe.broulik@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kwin.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "utils/memorymap.h"
#include "utils/ramfile.h"

#include <QScopeGuard>
#include <QSemaphore>
#include <QTemporaryFile>
#include <QTest>
#include <QThreadPool>

using namespace KWin;

class TestUtils : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testRamFile();
    void testSealedRamFile();
    void testUnmapWithBusyGlobalThreadPool();
    void testMemoryMapClosesItsFileDescriptor();
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
    QSKIP("Sealing requires memfd support.");
#endif
}

static bool isMapped(void *address)
{
#if defined(Q_OS_FREEBSD)
    char vec;
#else
    unsigned char vec;
#endif
    return mincore(address, 1, &vec) == 0;
}

void TestUtils::testUnmapWithBusyGlobalThreadPool()
{
    // Unmapping must not wait for other work on the global thread pool.
    QThreadPool *pool = QThreadPool::globalInstance();
    const int threadCount = pool->maxThreadCount();
    QSemaphore started;
    QSemaphore release;
    auto cleanup = qScopeGuard([&]() {
        release.release(threadCount);
        pool->waitForDone();
    });
    for (int i = 0; i < threadCount; ++i) {
        pool->start([&]() {
            started.release();
            release.acquire();
        });
    }
    QVERIFY(started.tryAcquire(threadCount, 5000));

    void *address;
    {
        MemoryMap map(1 << 20, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        QVERIFY(map.isValid());
        address = map.data();
        QVERIFY(isMapped(address));
    }
    QTRY_VERIFY(!isMapped(address));
}

static bool isSameFile(int fd, int otherFd)
{
    struct stat st;
    struct stat otherSt;
    if (fstat(fd, &st) != 0 || fstat(otherFd, &otherSt) != 0) {
        return false;
    }
    return st.st_dev == otherSt.st_dev && st.st_ino == otherSt.st_ino;
}

void TestUtils::testMemoryMapClosesItsFileDescriptor()
{
    // The map keeps a duplicate of the file descriptor and closes it after unmapping.
    QTemporaryFile file;
    QVERIFY(file.open());
    QVERIFY(file.resize(1 << 20));
    const FileDescriptor fd(dup(file.handle()));
    QVERIFY(fd.isValid());

    // duplicate() takes the lowest free number, which is the one probed here.
    int keptFd = fd.duplicate().get();
    QVERIFY(keptFd != -1);
    QVERIFY(!isSameFile(keptFd, fd.get()));

    void *address;
    {
        MemoryMap map(1 << 20, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        QVERIFY(map.isValid());
        address = map.data();
        QVERIFY(isSameFile(keptFd, fd.get()));
    }
    QTRY_VERIFY(!isMapped(address));
    QTRY_VERIFY(!isSameFile(keptFd, fd.get()));
    QVERIFY(fcntl(fd.get(), F_GETFD) != -1);
}

QTEST_MAIN(TestUtils)
#include "test_utils.moc"
