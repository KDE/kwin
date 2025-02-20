#include <QtDBus>

#include "kwin_wayland_test.h"

#include "scripting/scripting.h"
#include "wayland_server.h"

using namespace KWin;

static const QString s_socketName = QStringLiteral("test_kwin_scripting-0");
static const QString s_serviceName = QStringLiteral("org.kde.kwin.ScriptingTest");

// Mock DBus interface for testing
class MockDBusInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.ScriptingTest")

public:
    MockDBusInterface(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

Q_SIGNALS:
    void callWithIntCalled(int value);
    void callWithDoubleCalled(double value);

    void callWithByteCalled(quint8 value);
    void callWithInt16Called(qint16 value);
    void callWithUint16Called(quint16 value);
    void callWithInt32Called(qint32 value);
    void callWithUint32Called(quint32 value);
    void callWithInt64Called(qint64 value);
    void callWithUint64Called(quint64 value);

public Q_SLOTS:
    int callWithInt(int value)
    {
        Q_UNUSED(value);
        Q_EMIT callWithIntCalled(value);
        return value;
    }

    double callWithDouble(double value)
    {
        Q_UNUSED(value);
        Q_EMIT callWithDoubleCalled(value);
        return value;
    }

    quint8 callWithByte(quint8 value)
    {
        Q_UNUSED(value);
        Q_EMIT callWithByteCalled(value);
        return value;
    }

    qint16 callWithInt16(qint16 value)
    {
        Q_UNUSED(value);
        Q_EMIT callWithInt16Called(value);
        return value;
    }

    quint16 callWithUint16(quint16 value)
    {
        Q_UNUSED(value);
        Q_EMIT callWithUint16Called(value);
        return value;
    }

    qint32 callWithInt32(qint32 value)
    {
        Q_UNUSED(value);
        Q_EMIT callWithInt32Called(value);
        return value;
    }

    quint32 callWithUint32(quint32 value)
    {
        Q_UNUSED(value);
        Q_EMIT callWithUint32Called(value);
        return value;
    }

    qint64 callWithInt64(qint64 value)
    {
        Q_UNUSED(value);
        Q_EMIT callWithInt64Called(value);
        return value;
    }

    quint64 callWithUint64(quint64 value)
    {
        Q_UNUSED(value);
        Q_EMIT callWithUint64Called(value);
        return value;
    }
};

class ScriptingTest : public QObject
{
    Q_OBJECT

private:
    MockDBusInterface dbusInterface;

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void callDBus_data();

    /**
     * Test the `callDBus` method, and ensure it can call DBus methods with
     * various argument types.
     */
    void callDBus();
};

void ScriptingTest::initTestCase()
{
    QVERIFY(waylandServer()->init(s_socketName));

    kwinApp()->start();
    QVERIFY(Scripting::self());

    // Register the mock DBus interface
    QDBusConnection::sessionBus().registerService(s_serviceName);
    QDBusConnection::sessionBus().registerObject("/", &dbusInterface, QDBusConnection::ExportAllSlots);
}

void ScriptingTest::cleanupTestCase()
{
    // Unregister the mock DBus interface
    QDBusConnection::sessionBus().unregisterObject("/");
    QDBusConnection::sessionBus().unregisterService(s_serviceName);

    // Stop the application
    kwinApp()->quit();
}

void ScriptingTest::callDBus_data()
{
    QTest::addColumn<QString>("scriptName");
    QTest::addColumn<QString>("signalName");
    QTest::addColumn<QVariant>("expectedResult");

    QTest::newRow("callWithInt") << "int" << "callWithIntCalled(int)" << QVariant::fromValue<int>(42);
    QTest::newRow("callWithDouble") << "double" << "callWithDoubleCalled(double)" << QVariant::fromValue<double>(42.42);

    QTest::newRow("callWithByte") << "byte" << "callWithByteCalled(quint8)" << QVariant::fromValue<quint8>(42);
    QTest::newRow("callWithInt16") << "int16" << "callWithInt16Called(qint16)" << QVariant::fromValue<qint16>(42);
    QTest::newRow("callWithUint16") << "uint16" << "callWithUint16Called(quint16)" << QVariant::fromValue<quint16>(42);
    QTest::newRow("callWithInt32") << "int32" << "callWithInt32Called(qint32)" << QVariant::fromValue<qint32>(42);
    QTest::newRow("callWithUint32") << "uint32" << "callWithUint32Called(quint32)" << QVariant::fromValue<quint32>(42);
    QTest::newRow("callWithInt64") << "int64" << "callWithInt64Called(qint64)" << QVariant::fromValue<qint64>(42);
    QTest::newRow("callWithUint64") << "uint64" << "callWithUint64Called(quint64)" << QVariant::fromValue<quint64>(42);
}

void ScriptingTest::callDBus()
{

    QFETCH(QString, scriptName);
    QFETCH(QString, signalName);
    QFETCH(QVariant, expectedResult);

    // Find the script to load
    const QString scriptToLoad = QFINDTESTDATA("./scripts/call_dbus/" + scriptName + ".js");
    QVERIFY(!scriptToLoad.isEmpty());

    // Check if the signal exists in dbusInterface
    QByteArray signalNameArray = signalName.toLatin1();
    const char *signal = signalNameArray.constData();
    int signalIndex = dbusInterface.metaObject()->indexOfSignal(signal);
    QVERIFY(signalIndex != -1); // This will fail if the signal does not exist

    // Get the signal's QMetaMethod
    QMetaMethod signalMethod = dbusInterface.metaObject()->method(signalIndex);

    // Connect to the signal
    QSignalSpy spy(&dbusInterface, signalMethod);
    QVERIFY(spy.isValid());

    // Load the script
    QTRY_VERIFY(Scripting::self()->loadScript(scriptToLoad));
    QVERIFY(Scripting::self()->isScriptLoaded(scriptToLoad));
    AbstractScript *script = Scripting::self()->findScript(scriptToLoad);
    QVERIFY(script);

    // Run the script
    QSignalSpy runningChangedSpy(script, &AbstractScript::runningChanged);
    script->run();

    // Wait to confirm the script is running
    QVERIFY(runningChangedSpy.wait());
    QCOMPARE(runningChangedSpy.count(), 1);
    QCOMPARE(runningChangedSpy.first().first().toBool(), true);

    // Confirm the script successfully called the target dbus method
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().value<QVariant>(), expectedResult);
}

WAYLANDTEST_MAIN(ScriptingTest)
#include "scripting_test.moc"
