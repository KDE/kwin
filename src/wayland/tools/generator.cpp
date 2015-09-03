/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "generator.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QBuffer>
#include <QByteArray>
#include <QFutureWatcher>
#include <QMutexLocker>
#include <QDate>
#include <QProcess>
#include <QtConcurrent>
#include <QTextStream>

namespace KWayland
{
namespace Tools
{

Generator::Generator(QObject *parent)
    : QObject(parent)
{
}

Generator::~Generator() = default;

void Generator::start()
{
    startAuthorNameProcess();
    startAuthorEmailProcess();

    startGenerateHeaderFile();
    startGenerateCppFile();
}

void Generator::startGenerateHeaderFile()
{
    QFutureWatcher<void> *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, &Generator::checkEnd);
    m_finishedCounter++;
    watcher->setFuture(QtConcurrent::run([this] {
        QFile file(QStringLiteral("%1.h").arg(m_baseFileName));
        file.open(QIODevice::WriteOnly);
        m_stream.setLocalData(new QTextStream(&file));
        generateCopyrightHeader();
        generateStartIncludeGuard();
        generateHeaderIncludes();
        generateWaylandForwardDeclarations();
        generateStartNamespace();
        generateNamespaceForwardDeclarations();
        generateClass();
        generateEndNamespace();
        generateEndIncludeGuard();

        m_stream.setLocalData(nullptr);
        file.close();
    }));
}

void Generator::startGenerateCppFile()
{
    QFutureWatcher<void> *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, &Generator::checkEnd);
    m_finishedCounter++;
    watcher->setFuture(QtConcurrent::run([this] {
        QFile file(QStringLiteral("%1.cpp").arg(m_baseFileName));
        file.open(QIODevice::WriteOnly);
        m_stream.setLocalData(new QTextStream(&file));
        generateCopyrightHeader();
        generateCppIncludes();
        generateStartNamespace();
        generatePrivateClass();
        generateClientCpp();

        generateEndNamespace();

        m_stream.setLocalData(nullptr);
        file.close();
    }));
}

void Generator::checkEnd()
{
    m_finishedCounter--;
    if (m_finishedCounter == 0) {
        QCoreApplication::quit();
    }
}

void Generator::startAuthorNameProcess()
{
    QProcess *git = new QProcess(this);
    git->setArguments(QStringList{
        QStringLiteral("config"),
        QStringLiteral("--get"),
        QStringLiteral("user.name")
    });
    git->setProgram(QStringLiteral("git"));
    connect(git, static_cast<void (QProcess::*)(int)>(&QProcess::finished), this,
        [this, git] {
            QMutexLocker locker(&m_mutex);
            m_authorName = QString::fromLocal8Bit(git->readAllStandardOutput()).trimmed();
            git->deleteLater();
            m_waitCondition.wakeAll();
        }
    );
    git->start();
}

void Generator::startAuthorEmailProcess()
{
    QProcess *git = new QProcess(this);
    git->setArguments(QStringList{
        QStringLiteral("config"),
        QStringLiteral("--get"),
        QStringLiteral("user.email")
    });
    git->setProgram(QStringLiteral("git"));
    connect(git, static_cast<void (QProcess::*)(int)>(&QProcess::finished), this,
        [this, git] {
            QMutexLocker locker(&m_mutex);
            m_authorEmail = QString::fromLocal8Bit(git->readAllStandardOutput()).trimmed();
            git->deleteLater();
            m_waitCondition.wakeAll();
        }
    );
    git->start();
}

void Generator::generateCopyrightHeader()
{
    m_mutex.lock();
    while (m_authorEmail.isEmpty() || m_authorName.isEmpty()) {
        m_waitCondition.wait(&m_mutex);
    }
    m_mutex.unlock();
    const QString templateString = QStringLiteral(
"/****************************************************************************\n"
"Copyright %1  %2 <%3>\n"
"\n"
"This library is free software; you can redistribute it and/or\n"
"modify it under the terms of the GNU Lesser General Public\n"
"License as published by the Free Software Foundation; either\n"
"version 2.1 of the License, or (at your option) version 3, or any\n"
"later version accepted by the membership of KDE e.V. (or its\n"
"successor approved by the membership of KDE e.V.), which shall\n"
"act as a proxy defined in Section 6 of version 3 of the license.\n"
"\n"
"This library is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
"Lesser General Public License for more details.\n"
"\n"
"You should have received a copy of the GNU Lesser General Public\n"
"License along with this library.  If not, see <http://www.gnu.org/licenses/>.\n"
"****************************************************************************/\n"
);
    QDate date = QDate::currentDate();
    *m_stream.localData() << templateString.arg(date.year()).arg(m_authorName).arg(m_authorEmail);
}

void Generator::generateEndIncludeGuard()
{
    *m_stream.localData() << QStringLiteral("#endif\n");
}

void Generator::generateStartIncludeGuard()
{
    const QString templateString = QStringLiteral(
"#ifndef KWAYLAND_%1_%2_H\n"
"#define KWAYLAND_%1_%2_H\n\n");

    *m_stream.localData() << templateString.arg(projectToName().toUpper()).arg(m_className.toUpper());
}

void Generator::generateStartNamespace()
{
    const QString templateString = QStringLiteral(
"namespace KWayland\n"
"{\n"
"namespace %1\n"
"{\n\n");
    *m_stream.localData() << templateString.arg(projectToName());
}

void Generator::generateEndNamespace()
{
    *m_stream.localData() << QStringLiteral("\n}\n}\n\n");
}

void Generator::generateHeaderIncludes()
{
    switch (m_project) {
    case Project::Client:
        *m_stream.localData() << QStringLiteral("#include <QObject>\n\n");
        break;
    case Project::Server:
        // TODO: implement
        break;
    default:
        Q_UNREACHABLE();
    }
    *m_stream.localData() << QStringLiteral("#include <KWayland/%1/kwayland%2_export.h>\n\n").arg(projectToName()).arg(projectToName().toLower());
}

void Generator::generateCppIncludes()
{
    *m_stream.localData() << QStringLiteral("#include \"%1.h\"\n").arg(m_className.toLower());
    switch (m_project) {
    case Project::Client:
        *m_stream.localData() << QStringLiteral("#include \"event_queue.h\"\n\n");
        break;
    case Project::Server:
        // TODO: implement
        break;
    default:
        Q_UNREACHABLE();
    }
}

void Generator::generateClass()
{
    switch (m_project) {
    case Project::Client:
        generateClientClassStart();
        generateClientClassCasts();
        generateClientClassSignals();
        generateClientClassEnd();
        break;
    case Project::Server:
        // TODO: implement
        break;
    default:
        Q_UNREACHABLE();
    }
}

void Generator::generatePrivateClass()
{
    switch (m_project) {
    case Project::Client:
        generateClientPrivateClass();
        break;
    case Project::Server:
        // TODO: implement
        break;
    default:
        Q_UNREACHABLE();
    }
}

void Generator::generateClientPrivateClass()
{
    const QString templateString = QStringLiteral(
"class %1::Private\n"
"{\n"
"public:\n"
"    Private() = default;\n"
"\n"
"    WaylandPointer<%2, %2_destroy> %3;\n"
"    EventQueue *queue = nullptr;\n"
"};\n\n");

    *m_stream.localData() << templateString.arg(m_className).arg(m_waylandGlobal).arg(m_className.toLower());
}

void Generator::generateClientCpp()
{
    const QString templateString = QStringLiteral(
"%1::%1(QObject *parent)\n"
"    : QObject(parent)\n"
"    , d(new Private)\n"
"{\n"
"}\n"
"\n"
"%1::~%1()\n"
"{\n"
"    release();\n"
"}\n"
"\n"
"void %1::setup(%3 *%2)\n"
"{\n"
"    Q_ASSERT(%2);\n"
"    Q_ASSERT(!d->%2);\n"
"    d->%2.setup(%2);\n"
"}\n"
"\n"
"void %1::release()\n"
"{\n"
"    d->%2.release();\n"
"}\n"
"\n"
"void %1::destroy()\n"
"{\n"
"    d->%2.destroy();\n"
"}\n"
"\n"
"void %1::setEventQueue(EventQueue *queue)\n"
"{\n"
"    d->queue = queue;\n"
"}\n"
"\n"
"EventQueue *%1::eventQueue()\n"
"{\n"
"    return d->queue;\n"
"}\n"
"\n"
"%1::operator %3*() {\n"
"    return d->%2;\n"
"}\n"
"\n"
"%1::operator %3*() const {\n"
"    return d->%2;\n"
"}\n"
"\n"
"bool %1::isValid() const\n"
"{\n"
"    return d->%2.isValid();\n"
"}\n\n");

    *m_stream.localData() << templateString.arg(m_className).arg(m_className.toLower()).arg(m_waylandGlobal);
}

void Generator::generateClientClassStart()
{
    const QString templateString = QStringLiteral(
"/**\n"
" * @short Wrapper for the %2 interface.\n"
" *\n"
" * This class provides a convenient wrapper for the %2 interface.\n"
" *\n"
" * To use this class one needs to interact with the Registry. There are two\n"
" * possible ways to create the %1 interface:\n"
" * @code\n"
" * %1 *c = registry->create%1(name, version);\n"
" * @endcode\n"
" *\n"
" * This creates the %1 and sets it up directly. As an alternative this\n"
" * can also be done in a more low level way:\n"
" * @code\n"
" * %1 *c = new %1;\n"
" * c->setup(registry->bind%1(name, version));\n"
" * @endcode\n"
" *\n"
" * The %1 can be used as a drop-in replacement for any %2\n"
" * pointer as it provides matching cast operators.\n"
" *\n"
" * @see Registry\n"
" **/\n"
"class KWAYLAND%3_EXPORT %1 : public QObject\n"
"{\n"
"    Q_OBJECT\n"
"public:\n"
"    /**\n"
"     * Creates a new %1.\n"
"     * Note: after constructing the %1 it is not yet valid and one needs\n"
"     * to call setup. In order to get a ready to use %1 prefer using\n"
"     * Registry::create%1.\n"
"     **/\n"
"    explicit %1(QObject *parent = nullptr);\n"
"    virtual ~%1();\n"
"\n"
"    /**\n"
"     * @returns @c true if managing a %2.\n"
"     **/\n"
"    bool isValid() const;\n"
"    /**\n"
"     * Setup this %1 to manage the @p %4.\n"
"     * When using Registry::create%1 there is no need to call this\n"
"     * method.\n"
"     **/\n"
"    void setup(%2 *%4);\n"
"    /**\n"
"     * Releases the %2 interface.\n"
"     * After the interface has been released the %1 instance is no\n"
"     * longer valid and can be setup with another %2 interface.\n"
"     **/\n"
"    void release();\n"
"    /**\n"
"     * Destroys the data hold by this %1.\n"
"     * This method is supposed to be used when the connection to the Wayland\n"
"     * server goes away. If the connection is not valid any more, it's not\n"
"     * possible to call release any more as that calls into the Wayland\n"
"     * connection and the call would fail. This method cleans up the data, so\n"
"     * that the instance can be deleted or setup to a new %2 interface\n"
"     * once there is a new connection available.\n"
"     *\n"
"     * It is suggested to connect this method to ConnectionThread::connectionDied:\n"
"     * @code\n"
"     * connect(connection, &ConnectionThread::connectionDied, %4, &%1::destroy);\n"
"     * @endcode\n"
"     *\n"
"     * @see release\n"
"     **/\n"
"    void destroy();\n"
"\n"
"    /**\n"
"     * Sets the @p queue to use for creating objects with this %1.\n"
"     **/\n"
"    void setEventQueue(EventQueue *queue);\n"
"    /**\n"
"     * @returns The event queue to use for creating objects with this %1.\n"
"     **/\n"
"    EventQueue *eventQueue();\n\n");
    *m_stream.localData() << templateString.arg(m_className).arg(m_waylandGlobal).arg(projectToName().toUpper()).arg(m_className.toLower());
}

void Generator::generateClientClassCasts()
{
    const QString templateString = QStringLiteral(
"    operator %1*();\n"
"    operator %1*() const;\n\n");
    *m_stream.localData() << templateString.arg(m_waylandGlobal);
}

void Generator::generateClientClassEnd()
{
    *m_stream.localData() << QStringLiteral(
"private:\n"
"    class Private;\n"
"    QScopedPointer<Private> d;\n"
"};\n\n");
}

void Generator::generateWaylandForwardDeclarations()
{
    *m_stream.localData() << QStringLiteral("struct %1;\n\n").arg(m_waylandGlobal);
}

void Generator::generateNamespaceForwardDeclarations()
{
    switch (m_project) {
    case Project::Client:
        *m_stream.localData() << QStringLiteral("class EventQueue;\n\n");
        break;
    case Project::Server:
        // TODO: implement
        break;
    default:
        Q_UNREACHABLE();
    }
}

void Generator::generateClientClassSignals()
{
    const QString templateString = QStringLiteral(
"Q_SIGNALS:\n"
"    /**\n"
"     * The corresponding global for this interface on the Registry got removed.\n"
"     *\n"
"     * This signal gets only emitted if the %1 got created by\n"
"     * Registry::create%1\n"
"     **/\n"
"    void removed();\n\n");
    *m_stream.localData() << templateString.arg(m_className);
}

QString Generator::projectToName() const
{
    switch (m_project) {
    case Project::Client:
        return QStringLiteral("Client");
    case Project::Server:
        return QStringLiteral("Server");
    default:
        Q_UNREACHABLE();
    }
}

}
}

int main(int argc, char **argv)
{
    using namespace KWayland::Tools;
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    QCommandLineOption className(QStringList{QStringLiteral("c"), QStringLiteral("class")},
                                 QStringLiteral("The class name to generate. On Server \"Interface\" is added."),
                                 QStringLiteral("ClassName"));
    QCommandLineOption fileName(QStringList{QStringLiteral("f"), QStringLiteral("file")},
                                QStringLiteral("The base name of files to be generated. E.g. for \"foo\" the files \"foo.h\" and \"foo.cpp\" are generated"),
                                QStringLiteral("FileName"));
    QCommandLineOption globalName(QStringList{QStringLiteral("g"), QStringLiteral("global")},
                                 QStringLiteral("The name for the Wayland global for which the wrapper is generated. E.g. \"wl_foo\"."),
                                 QStringLiteral("wl_global"));

    parser.addHelpOption();
    parser.addOption(className);
    parser.addOption(fileName);
    parser.addOption(globalName);

    parser.process(app);

    Generator generator(&app);
    generator.setClassName(parser.value(className));
    generator.setBaseFileName(parser.value(fileName));
    generator.setWaylandGlobal(parser.value(globalName));
    generator.setProject(Generator::Project::Client);
    generator.start();

    return app.exec();
}
