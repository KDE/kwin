#include <QTest>
#include <QObject>
#include <QTemporaryFile>

#include "ftracemarker.h"

class TestFTrace : public QObject
{
    Q_OBJECT
public:
    TestFTrace();
private Q_SLOTS:
    void benchmarkTraceOff();
    void benchmarkTemplate();
    void benchmarkTraceDurationOff();
    void enable();
private:
    QTemporaryFile m_tempFile;
};

TestFTrace::TestFTrace()
{
    m_tempFile.open();
    qputenv("KWIN_PERF_FTRACE_FILE", m_tempFile.fileName().toLatin1());
    
    KWin::FTraceLogger::create();
}

void TestFTrace::benchmarkTraceOff()
{
    // this macro should no-op, so take no time at all
    QBENCHMARK{
        fTrace("BENCH", 123, "foo");
    }
}

void TestFTrace::benchmarkTemplate()
{
    // this macro should no-op, so take no time at all
    QBENCHMARK{
        KWin::FTraceLogger::self()->trace("asdfa", 234, "asdfs");
    }
}

void TestFTrace::benchmarkTraceDurationOff()
{
    QBENCHMARK{
        fTraceDuration("BENCH", 123, "foo");
    }
}

void TestFTrace::enable()
{
    KWin::FTraceLogger::self()->setEnabled(true);
    QVERIFY(KWin::FTraceLogger::self()->isEnabled());
    
    {
    fTrace("TEST", 123, "foo");
    fTraceDuration("TEST_DURATION", "boo");
    fTrace("TEST", 123, "foo");
    }
    
    QCOMPARE( m_tempFile.readLine(), "TEST123foo\n");
    QCOMPARE( m_tempFile.readLine(), "TEST_DURATIONboo begin_ctx=1\n");
    QCOMPARE( m_tempFile.readLine(), "TEST123foo\n");
    QCOMPARE( m_tempFile.readLine(), "TEST_DURATIONboo end_ctx=1\n");
        
}

QTEST_MAIN(TestFTrace)

#include "test_ftrace.moc"
