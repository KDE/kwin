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
#include <QXmlStreamReader>

#include <QDebug>

namespace KWayland
{
namespace Tools
{

static QMap<QString, QString> s_clientClassNameMapping;

static QString toQtInterfaceName(const QString &wlInterface)
{
    auto it = s_clientClassNameMapping.constFind(wlInterface);
    if (it != s_clientClassNameMapping.constEnd()) {
        return it.value();
    } else {
        qWarning() << "Cannot find mapping for " << wlInterface;
    }
    return wlInterface;
}

static QString toCamelCase(const QString &underscoreName)
{
    const QStringList parts = underscoreName.split(QStringLiteral("_"));
    if (parts.count() < 2) {
        return underscoreName;
    }
    auto it = parts.constBegin();
    QString camelCase = (*it);
    it++;
    for (; it != parts.constEnd(); ++it) {
        camelCase.append((*it).left(1).toUpper());
        camelCase.append((*it).mid(1));
    }
    return camelCase;
}

Argument::Argument()
{
}

Argument::Argument(const QXmlStreamAttributes &attributes)
    : m_name(attributes.value(QStringLiteral("name")).toString())
    , m_type(parseType(attributes.value(QStringLiteral("type"))))
    , m_allowNull(attributes.hasAttribute(QStringLiteral("allow-null")))
    , m_inteface(attributes.value(QStringLiteral("interface")).toString())
{
}

Argument::~Argument() = default;

Argument::Type Argument::parseType(const QStringRef &type)
{
    if (type.compare(QLatin1String("new_id")) == 0) {
        return Type::NewId;
    }
    if (type.compare(QLatin1String("destructor")) == 0) {
        return Type::Destructor;
    }
    if (type.compare(QLatin1String("object")) == 0) {
        return Type::Object;
    }
    if (type.compare(QLatin1String("fd")) == 0) {
        return Type::FileDescriptor;
    }
    if (type.compare(QLatin1String("fixed")) == 0) {
        return Type::Fixed;
    }
    if (type.compare(QLatin1String("uint")) == 0) {
        return Type::Uint;
    }
    if (type.compare(QLatin1String("int")) == 0) {
        return Type::Int;
    }
    if (type.compare(QLatin1String("string")) == 0) {
        return Type::String;
    }

    return Type::Unknown;
}

QString Argument::typeAsQt() const
{
    switch (m_type) {
    case Type::Destructor:
        return QString();
    case Type::FileDescriptor:
        return QStringLiteral("int");
    case Type::Fixed:
        return QStringLiteral("qreal");
    case Type::Int:
        return QStringLiteral("qint32");
    case Type::NewId:
    case Type::Object:
        return toQtInterfaceName(m_inteface);
    case Type::String:
        return QStringLiteral("const QString &");
    case Type::Uint:
        return QStringLiteral("quint32");
    case Type::Unknown:
        return QString();
    default:
        Q_UNREACHABLE();
    }
}

QString Argument::typeAsServerWl() const
{
    switch (m_type) {
    case Type::Destructor:
        return QString();
    case Type::FileDescriptor:
        return QStringLiteral("int32_t");
    case Type::Fixed:
        return QStringLiteral("wl_fixed");
    case Type::Int:
        return QStringLiteral("int32_t");
    case Type::Object:
        return QStringLiteral("wl_resource *");
    case Type::String:
        return QStringLiteral("const char *");
    case Type::Uint:
    case Type::NewId:
        return QStringLiteral("uint32_t");
    case Type::Unknown:
        return QString();
    default:
        Q_UNREACHABLE();
    }
}

Request::Request()
{
}

Request::Request(const QString &name)
    : m_name(name)
{
}

Request::~Request() = default;

bool Request::isFactory() const
{
    for (const auto a: m_arguments) {
        if (a.type() == Argument::Type::NewId) {
            return true;
        }
    }
    return false;
}

Event::Event()
{
}

Event::Event(const QString &name)
    : m_name(name)
{
}

Event::~Event() = default;

Interface::Interface() = default;

Interface::Interface(const QXmlStreamAttributes &attributes)
    : m_name(attributes.value(QStringLiteral("name")).toString())
    , m_version(attributes.value(QStringLiteral("version")).toUInt())
    , m_factory(Q_NULLPTR)
{
    auto it = s_clientClassNameMapping.constFind(m_name);
    if (it != s_clientClassNameMapping.constEnd()) {
        m_clientName = it.value();
    } else {
        qWarning() << "Failed to map " << m_name << " to a KWayland name";
    }
}

Interface::~Interface() = default;

Generator::Generator(QObject *parent)
    : QObject(parent)
{
}

Generator::~Generator() = default;

void Generator::start()
{
    startAuthorNameProcess();
    startAuthorEmailProcess();

    startParseXml();

    startGenerateHeaderFile();
    startGenerateCppFile();
    startGenerateServerHeaderFile();
    startGenerateServerCppFile();
}

void Generator::startParseXml()
{
    if (m_xmlFileName.isEmpty()) {
        return;
    }
    QFile xmlFile(m_xmlFileName);
    xmlFile.open(QIODevice::ReadOnly);
    m_xmlReader.setDevice(&xmlFile);
    while (!m_xmlReader.atEnd()) {
        if (!m_xmlReader.readNextStartElement()) {
            continue;
        }
        if (m_xmlReader.qualifiedName().compare(QLatin1String("protocol")) == 0) {
            parseProtocol();
        }
    }

    auto findFactory = [this] (const QString interfaceName) -> Interface* {
        for (auto it = m_interfaces.begin(); it != m_interfaces.end(); ++it) {
            if ((*it).name().compare(interfaceName) == 0) {
                continue;
            }
            for (auto r: (*it).requests()) {
                for (auto a: r.arguments()) {
                    if (a.type() == Argument::Type::NewId && a.interface().compare(interfaceName) == 0) {
                        return &(*it);
                    }
                }
            }
        }
        return nullptr;
    };
    for (auto it = m_interfaces.begin(); it != m_interfaces.end(); ++it) {
        Interface *factory = findFactory((*it).name());
        if (factory) {
            qDebug() << (*it).name() << "gets factored by" << factory->kwaylandClientName();
            (*it).setFactory(factory);
        } else {
            qDebug() << (*it).name() << "considered as a global";
            (*it).markAsGlobal();
        }
    }
}

void Generator::parseProtocol()
{
    const auto attributes = m_xmlReader.attributes();
    const QString protocolName = attributes.value(QStringLiteral("name")).toString();

    if (m_baseFileName.isEmpty()) {
        m_baseFileName = protocolName.toLower();
    }

    while (!m_xmlReader.atEnd()) {
        if (!m_xmlReader.readNextStartElement()) {
            if (m_xmlReader.qualifiedName().compare(QLatin1String("protocol")) == 0) {
                return;
            }
            continue;
        }
        if (m_xmlReader.qualifiedName().compare(QLatin1String("interface")) == 0) {
            m_interfaces << parseInterface();
        }
    }
}

Interface Generator::parseInterface()
{
    Interface interface(m_xmlReader.attributes());
    while (!m_xmlReader.atEnd()) {
        if (!m_xmlReader.readNextStartElement()) {
            if (m_xmlReader.qualifiedName().compare(QLatin1String("interface")) == 0) {
                break;
            }
            continue;
        }
        if (m_xmlReader.qualifiedName().compare(QLatin1String("request")) == 0) {
            interface.addRequest(parseRequest());
        }
        if (m_xmlReader.qualifiedName().compare(QLatin1String("event")) == 0) {
            interface.addEvent(parseEvent());
        }
    }
    return interface;
}

Request Generator::parseRequest()
{
    const auto attributes = m_xmlReader.attributes();
    Request request(attributes.value(QStringLiteral("name")).toString());
    if (attributes.value(QStringLiteral("type")).toString().compare(QLatin1String("destructor")) == 0) {
        request.markAsDestructor();
    }
    while (!m_xmlReader.atEnd()) {
        if (!m_xmlReader.readNextStartElement()) {
            if (m_xmlReader.qualifiedName().compare(QLatin1String("request")) == 0) {
                break;
            }
            continue;
        }
        if (m_xmlReader.qualifiedName().compare(QLatin1String("arg")) == 0) {
            request.addArgument(Argument(m_xmlReader.attributes()));
        }
    }
    return request;
}

Event Generator::parseEvent()
{
    const auto attributes = m_xmlReader.attributes();
    Event event(attributes.value(QStringLiteral("name")).toString());
    while (!m_xmlReader.atEnd()) {
        if (!m_xmlReader.readNextStartElement()) {
            if (m_xmlReader.qualifiedName().compare(QLatin1String("event")) == 0) {
                break;
            }
            continue;
        }
        if (m_xmlReader.qualifiedName().compare(QLatin1String("arg")) == 0) {
            event.addArgument(Argument(m_xmlReader.attributes()));
        }
    }
    return event;
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
        m_project.setLocalData(Project::Client);
        generateCopyrightHeader();
        generateStartIncludeGuard();
        generateHeaderIncludes();
        generateWaylandForwardDeclarations();
        generateStartNamespace();
        generateNamespaceForwardDeclarations();
        for (auto it = m_interfaces.constBegin(); it != m_interfaces.constEnd(); ++it) {
            generateClass(*it);
        }
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
        m_project.setLocalData(Project::Client);
        generateCopyrightHeader();
        generateCppIncludes();
        generateStartNamespace();
        for (auto it = m_interfaces.constBegin(); it != m_interfaces.constEnd(); ++it) {
            generatePrivateClass(*it);
            generateClientCpp(*it);
            generateClientCppRequests(*it);
        }

        generateEndNamespace();

        m_stream.setLocalData(nullptr);
        file.close();
    }));
}

void Generator::startGenerateServerHeaderFile()
{
    QFutureWatcher<void> *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, &Generator::checkEnd);
    m_finishedCounter++;
    watcher->setFuture(QtConcurrent::run([this] {
        QFile file(QStringLiteral("%1_interface.h").arg(m_baseFileName));
        file.open(QIODevice::WriteOnly);
        m_stream.setLocalData(new QTextStream(&file));
        m_project.setLocalData(Project::Server);
        generateCopyrightHeader();
        generateStartIncludeGuard();
        generateHeaderIncludes();
        generateStartNamespace();
        generateNamespaceForwardDeclarations();
        for (auto it = m_interfaces.constBegin(); it != m_interfaces.constEnd(); ++it) {
            generateClass(*it);
        }
        generateEndNamespace();
        generateEndIncludeGuard();

        m_stream.setLocalData(nullptr);
        file.close();
    }));
}

void Generator::startGenerateServerCppFile()
{
    QFutureWatcher<void> *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, &Generator::checkEnd);
    m_finishedCounter++;
    watcher->setFuture(QtConcurrent::run([this] {
        QFile file(QStringLiteral("%1_interface.cpp").arg(m_baseFileName));
        file.open(QIODevice::WriteOnly);
        m_stream.setLocalData(new QTextStream(&file));
        m_project.setLocalData(Project::Server);
        generateCopyrightHeader();
        generateCppIncludes();
        generateStartNamespace();
        for (auto it = m_interfaces.constBegin(); it != m_interfaces.constEnd(); ++it) {
            generatePrivateClass(*it);
//             generateClientCpp(*it);
//             generateClientCppRequests(*it);
        }

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

    *m_stream.localData() << templateString.arg(projectToName().toUpper()).arg(m_baseFileName.toUpper());
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
    switch (m_project.localData()) {
    case Project::Client:
        *m_stream.localData() << QStringLiteral("#include <QObject>\n\n");
        break;
    case Project::Server:
        *m_stream.localData() << QStringLiteral(
"#include \"global.h\"\n"
"#include \"resource.h\"\n\n");
        break;
    default:
        Q_UNREACHABLE();
    }
    *m_stream.localData() << QStringLiteral("#include <KWayland/%1/kwayland%2_export.h>\n\n").arg(projectToName()).arg(projectToName().toLower());
}

void Generator::generateCppIncludes()
{
    switch (m_project.localData()) {
    case Project::Client:
        *m_stream.localData() << QStringLiteral("#include \"%1.h\"\n").arg(m_baseFileName.toLower());
        *m_stream.localData() << QStringLiteral("#include \"event_queue.h\"\n");
        *m_stream.localData() << QStringLiteral("#include \"wayland_pointer_p.h\"\n\n");
        break;
    case Project::Server:
        *m_stream.localData() << QStringLiteral("#include \"%1_interface.h\"\n").arg(m_baseFileName.toLower());
        *m_stream.localData() << QStringLiteral(
"#include \"display.h\"\n"
"#include \"global_p.h\"\n"
"#include \"resource_p.h\"\n\n");
        break;
    default:
        Q_UNREACHABLE();
    }
}

void Generator::generateClass(const Interface &interface)
{
    switch (m_project.localData()) {
    case Project::Client:
        if (interface.isGlobal()) {
            generateClientGlobalClass(interface);
        } else {
            generateClientResourceClass(interface);
        }
        break;
    case Project::Server:
        if (interface.isGlobal()) {
            generateServerGlobalClass(interface);
        } else {
            generateServerResourceClass(interface);
        }
        break;
    default:
        Q_UNREACHABLE();
    }
}

void Generator::generateClientGlobalClass(const Interface &interface)
{
    generateClientGlobalClassDoxy(interface);
    generateClientClassQObjectDerived(interface);
    generateClientGlobalClassCtor(interface);
    generateClientClassDtor(interface);
    generateClientGlobalClassSetup(interface);
    generateClientClassReleaseDestroy(interface);
    generateClientClassStart(interface);
    generateClientClassRequests(interface);
    generateClientClassCasts(interface);
    generateClientClassSignals(interface);
    generateClientGlobalClassEnd(interface);
}

void Generator::generateClientResourceClass(const Interface &interface)
{
    generateClientClassQObjectDerived(interface);
    generateClientClassDtor(interface);
    generateClientResourceClassSetup(interface);
    generateClientClassReleaseDestroy(interface);
    generateClientClassRequests(interface);
    generateClientClassCasts(interface);
    generateClientResourceClassEnd(interface);
}

void Generator::generateServerGlobalClass(const Interface &interface)
{
    const QString templateString = QStringLiteral(
"class KWAYLANDSERVER_EXPORT %1 : public Global\n"
"{\n"
"    Q_OBJECT\n"
"public:\n"
"    virtual ~%1();\n"
"\n"
"private:\n"
"    explicit %1(Display *display, QObject *parent = nullptr);\n"
"    friend class Display;\n"
"    class Private;\n"
"};\n"
"\n");
    *m_stream.localData() << templateString.arg(interface.kwaylandServerName());
}

void Generator::generateServerResourceClass(const Interface &interface)
{
    const QString templateString = QStringLiteral(
"class KWAYLANDSERVER_EXPORT %1 : public Resource\n"
"{\n"
"    Q_OBJECT\n"
"public:\n"
"    virtual ~%1();\n"
"\n"
"private:\n"
"    explicit %1(%2 *parent, wl_resource *parentResource);\n"
"    friend class %2;\n"
"\n"
"    class Private;\n"
"    Private *d_func() const;\n"
"};\n"
"\n");
    *m_stream.localData() << templateString.arg(interface.kwaylandServerName()).arg(interface.factory()->kwaylandServerName());
}

void Generator::generatePrivateClass(const Interface &interface)
{
    switch (m_project.localData()) {
    case Project::Client:
        generateClientPrivateClass(interface);
        break;
    case Project::Server:
        if (interface.isGlobal()) {
            generateServerPrivateGlobalClass(interface);
        } else {
            generateServerPrivateResourceClass(interface);
        }
        break;
    default:
        Q_UNREACHABLE();
    }
}

void Generator::generateServerPrivateGlobalClass(const Interface &interface)
{
    QString templateString = QStringLiteral(
"class %1::Private : public Global::Private\n"
"{\n"
"public:\n"
"    Private(%1 *q, Display *d);\n"
"\n"
"private:\n"
"    void bind(wl_client *client, uint32_t version, uint32_t id) override;\n"
"\n"
"    static void unbind(wl_resource *resource);\n"
"    static Private *cast(wl_resource *r) {\n"
"        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));\n"
"    }\n"
"\n");
    *m_stream.localData() << templateString.arg(interface.kwaylandServerName());

    generateServerPrivateCallbackDefinitions(interface);

    templateString = QStringLiteral(
"    %1 *q;\n"
"    static const struct %2_interface s_interface;\n"
"    static const quint32 s_version;\n"
"};\n"
"\n"
"const quint32 %1::Private::s_version = %3;\n"
"\n");
    *m_stream.localData() << templateString.arg(interface.kwaylandServerName()).arg(interface.name()).arg(interface.version());
    generateServerPrivateInterfaceClass(interface);
    generateServerPrivateCallbackImpl(interface);
    generateServerPrivateGlobalCtorBindClass(interface);
}

void Generator::generateServerPrivateCallbackDefinitions(const Interface &interface)
{
    for (const auto &r: interface.requests()) {
        if (r.isDestructor() && !interface.isGlobal()) {
            continue;
        }
        *m_stream.localData() << QStringLiteral("    static void %1Callback(wl_client *client, wl_resource *resource").arg(toCamelCase(r.name()));
        for (const auto &a: r.arguments()) {
            *m_stream.localData() << QStringLiteral(", %1 %2").arg(a.typeAsServerWl()).arg(a.name());
        }
        *m_stream.localData() << QStringLiteral(");\n");
    }
    *m_stream.localData() << QStringLiteral("\n");
}

void Generator::generateServerPrivateCallbackImpl(const Interface &interface)
{
    for (const auto &r: interface.requests()) {
        if (r.isDestructor() && !interface.isGlobal()) {
            continue;
        }
        *m_stream.localData() << QStringLiteral("void %2::Private::%1Callback(wl_client *client, wl_resource *resource").arg(toCamelCase(r.name())).arg(interface.kwaylandServerName());
        for (const auto &a: r.arguments()) {
            *m_stream.localData() << QStringLiteral(", %1 %2").arg(a.typeAsServerWl()).arg(a.name());
        }
        *m_stream.localData() << QStringLiteral(
")\n"
"{\n");
        if (r.isDestructor()) {
            *m_stream.localData() << QStringLiteral(
"    Q_UNUSED(client)\n"
"    wl_resource_destroy(resource);\n");
        } else {
            *m_stream.localData() << QStringLiteral("    // TODO: implement\n");
        }
        *m_stream.localData() << QStringLiteral(
"}\n"
"\n");
    }
}

void Generator::generateServerPrivateGlobalCtorBindClass(const Interface &interface)
{
    QString templateString = QStringLiteral(
"%1::Private::Private(%1 *q, Display *d)\n"
"    : Global::Private(d, &%2_interface, s_version)\n"
"    , q(q)\n"
"{\n"
"}\n"
"\n"
"void %1::Private::bind(wl_client *client, uint32_t version, uint32_t id)\n"
"{\n"
"    auto c = display->getConnection(client);\n"
"    wl_resource *resource = c->createResource(&%2_interface, qMin(version, s_version), id);\n"
"    if (!resource) {\n"
"        wl_client_post_no_memory(client);\n"
"        return;\n"
"    }\n"
"    wl_resource_set_implementation(resource, &s_interface, this, unbind);\n"
"    // TODO: should we track?\n"
"}\n"
"\n"
"void %1::Private::unbind(wl_resource *resource)\n"
"{\n"
"    Q_UNUSED(resource)\n"
"    // TODO: implement?\n"
"}\n"
"\n");
    *m_stream.localData() << templateString.arg(interface.kwaylandServerName()).arg(interface.name());
}

void Generator::generateServerPrivateResourceClass(const Interface &interface)
{
    QString templateString = QStringLiteral(
"class %1::Private : public Resource::Private\n"
"{\n"
"public:\n"
"    Private(%1 *q, %2 *c, wl_resource *parentResource);\n"
"    ~Private();\n"
"\n"
"private:\n");
    *m_stream.localData() << templateString.arg(interface.kwaylandServerName()).arg(interface.factory()->kwaylandServerName());

    generateServerPrivateCallbackDefinitions(interface);

    templateString = QStringLiteral(
"    %1 *q_func() {\n"
"        return reinterpret_cast<%1 *>(q);\n"
"    }\n"
"\n"
"    static const struct %2_interface s_interface;\n"
"};\n"
"\n");
    *m_stream.localData() << templateString.arg(interface.kwaylandServerName()).arg(interface.name());

    generateServerPrivateInterfaceClass(interface);
    generateServerPrivateCallbackImpl(interface);
    generateServerPrivateResourceCtorDtorClass(interface);
}

void Generator::generateServerPrivateInterfaceClass(const Interface &interface)
{
    *m_stream.localData() << QStringLiteral("#ifndef DOXYGEN_SHOULD_SKIP_THIS\n");
    *m_stream.localData() << QStringLiteral("const struct %2_interface %1::Private::s_interface = {\n").arg(interface.kwaylandServerName()).arg(interface.name());
    bool first = true;
    for (auto r: interface.requests()) {
        if (!first) {
            *m_stream.localData() << QStringLiteral(",\n");
        } else {
            first = false;
        }
        if (r.isDestructor() && !interface.isGlobal()) {
            *m_stream.localData() << QStringLiteral("    resourceDestroyedCallback");
        } else {
            *m_stream.localData() << QStringLiteral("    %1Callback").arg(toCamelCase(r.name()));
        }
    }
    *m_stream.localData() << QStringLiteral("\n};\n#endif\n\n");
}

void Generator::generateServerPrivateResourceCtorDtorClass(const Interface &interface)
{
    QString templateString = QStringLiteral(
"%1::Private::Private(%1 *q, %2 *c, wl_resource *parentResource)\n"
"    : Resource::Private(q, c, parentResource, &%3_interface, &s_interface)\n"
"{\n"
"}\n"
"\n"
"%1::Private::~Private()\n"
"{\n"
"    if (resource) {\n"
"        wl_resource_destroy(resource);\n"
"        resource = nullptr;\n"
"    }\n"
"}\n");
    *m_stream.localData() << templateString.arg(interface.kwaylandServerName()).arg(interface.factory()->kwaylandServerName()).arg(interface.name());
}

void Generator::generateClientPrivateClass(const Interface &interface)
{
    if (interface.isGlobal()) {
        generateClientPrivateGlobalClass(interface);
    } else {
        generateClientPrivateResourceClass(interface);
    }

    const auto events = interface.events();
    if (!events.isEmpty()) {
        *m_stream.localData() << QStringLiteral("\nprivate:\n");
        // generate the callbacks
        for (auto event : events) {
            const QString templateString = QStringLiteral("    static void %1Callback(void *data, %2 *%2");
            *m_stream.localData() << templateString.arg(event.name()).arg(interface.name());
            const auto arguments = event.arguments();
            for (auto argument : arguments) {
                if (argument.interface().isNull()) {
                    *m_stream.localData() << QStringLiteral(", %1 %2").arg(argument.typeAsServerWl()).arg(argument.name());
                } else {
                    *m_stream.localData() << QStringLiteral(", %1 *%2").arg(argument.interface()).arg(argument.name());
                }
            }
            *m_stream.localData() << ");\n";
        }
        *m_stream.localData() << QStringLiteral("\n    static const %1_listener s_listener;\n").arg(interface.name());
    }

    *m_stream.localData() << QStringLiteral("};\n\n");
}

void Generator::generateClientPrivateResourceClass(const Interface &interface)
{
    const QString templateString = QStringLiteral(
"class %1::Private\n"
"{\n"
"public:\n"
"    Private(%1 *q);\n"
"\n"
"    void setup(%2 *arg);\n"
"\n"
"    WaylandPointer<%2, %2_destroy> %3;\n"
"\n"
"private:\n"
"    %1 *q;\n");

    *m_stream.localData() << templateString.arg(interface.kwaylandClientName()).arg(interface.name()).arg(interface.kwaylandClientName().toLower());
}

void Generator::generateClientPrivateGlobalClass(const Interface &interface)
{
    const QString templateString = QStringLiteral(
"class %1::Private\n"
"{\n"
"public:\n"
"    Private() = default;\n"
"\n"
"    void setup(%2 *arg);\n"
"\n"
"    WaylandPointer<%2, %2_destroy> %3;\n"
"    EventQueue *queue = nullptr;\n");

    *m_stream.localData() << templateString.arg(interface.kwaylandClientName()).arg(interface.name()).arg(interface.kwaylandClientName().toLower());
}

void Generator::generateClientCpp(const Interface &interface)
{
    // TODO: generate listener and callbacks
    const auto events = interface.events();
    if (!events.isEmpty()) {
        // listener
        *m_stream.localData() << QStringLiteral("const %1_listener %2::Private::s_listener = {\n").arg(interface.name()).arg(interface.kwaylandClientName());
        bool first = true;
        for (auto event : events) {
            if (!first) {
                *m_stream.localData() << QStringLiteral(",\n");
            }
            *m_stream.localData() << QStringLiteral("    %1Callback").arg(event.name());
            first = false;
        }
        *m_stream.localData() << QStringLiteral("\n};\n\n");

        // callbacks
        for (auto event : events) {
            *m_stream.localData() << QStringLiteral("void %1::Private::%2Callback(void *data, %3 *%3").arg(interface.kwaylandClientName()).arg(event.name()).arg(interface.name());

            const auto arguments = event.arguments();
            for (auto argument : arguments) {
                if (argument.interface().isNull()) {
                    *m_stream.localData() << QStringLiteral(", %1 %2").arg(argument.typeAsServerWl()).arg(argument.name());
                } else {
                    *m_stream.localData() << QStringLiteral(", %1 *%2").arg(argument.interface()).arg(argument.name());
                }
            }

            *m_stream.localData() << QStringLiteral(
")\n"
"{\n"
"    auto p = reinterpret_cast<%1::Private*>(data);\n"
"    Q_ASSERT(p->%2 == %3);\n").arg(interface.kwaylandClientName())
                               .arg(interface.kwaylandClientName().toLower())
                               .arg(interface.name());
            for (auto argument : arguments) {
                *m_stream.localData() << QStringLiteral("    Q_UNUSED(%1)\n").arg(argument.name());
            }
            *m_stream.localData() << QStringLiteral("    // TODO: implement\n}\n\n");
        }
    }

    if (interface.isGlobal()) {
        // generate ctor without this pointer to Private
        const QString templateString = QStringLiteral(
"%1::%1(QObject *parent)\n"
"    : QObject(parent)\n"
"    , d(new Private)\n"
"{\n"
"}\n");
        *m_stream.localData() << templateString.arg(interface.kwaylandClientName());
    } else {
        // Private ctor
        const QString templateString = QStringLiteral(
"%1::Private::Private(%1 *q)\n"
"    : q(q)\n"
"{\n"
"}\n"
"\n"
"%1::%1(QObject *parent)\n"
"    : QObject(parent)\n"
"    , d(new Private(this))\n"
"{\n"
"}\n");
        *m_stream.localData() << templateString.arg(interface.kwaylandClientName());
    }

    // setup call with optional add_listener
    const QString setupTemplate = QStringLiteral(
"\n"
"void %1::Private::setup(%3 *arg)\n"
"{\n"
"    Q_ASSERT(arg);\n"
"    Q_ASSERT(!%2);\n"
"    %2.setup(arg);\n");
    *m_stream.localData() << setupTemplate.arg(interface.kwaylandClientName())
                                           .arg(interface.kwaylandClientName().toLower())
                                           .arg(interface.name());
    if (!interface.events().isEmpty()) {
        *m_stream.localData() << QStringLiteral("    %1_add_listener(%2, &s_listener, this);\n").arg(interface.name()).arg(interface.kwaylandClientName().toLower());
    }
    *m_stream.localData() << QStringLiteral("}\n");

    const QString templateString = QStringLiteral(
"\n"
"%1::~%1()\n"
"{\n"
"    release();\n"
"}\n"
"\n"
"void %1::setup(%3 *%2)\n"
"{\n"
"    d->setup(%2);\n"
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

    *m_stream.localData() << templateString.arg(interface.kwaylandClientName())
                                           .arg(interface.kwaylandClientName().toLower())
                                           .arg(interface.name());
    if (interface.isGlobal()) {
        const QString templateStringGlobal = QStringLiteral(
"void %1::setEventQueue(EventQueue *queue)\n"
"{\n"
"    d->queue = queue;\n"
"}\n"
"\n"
"EventQueue *%1::eventQueue()\n"
"{\n"
"    return d->queue;\n"
"}\n\n"
        );
        *m_stream.localData() << templateStringGlobal.arg(interface.kwaylandClientName());
    }
}

void Generator::generateClientGlobalClassDoxy(const Interface &interface)
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
" **/\n");
    *m_stream.localData() << templateString.arg(interface.kwaylandClientName()).arg(interface.name());
}

void Generator::generateClientClassQObjectDerived(const Interface &interface)
{
    const QString templateString = QStringLiteral(
"class KWAYLANDCLIENT_EXPORT %1 : public QObject\n"
"{\n"
"    Q_OBJECT\n"
"public:\n");
    *m_stream.localData() << templateString.arg(interface.kwaylandClientName());
}

void Generator::generateClientGlobalClassCtor(const Interface &interface)
{
    const QString templateString = QStringLiteral(
"    /**\n"
"     * Creates a new %1.\n"
"     * Note: after constructing the %1 it is not yet valid and one needs\n"
"     * to call setup. In order to get a ready to use %1 prefer using\n"
"     * Registry::create%1.\n"
"     **/\n"
"    explicit %1(QObject *parent = nullptr);\n");
    *m_stream.localData() << templateString.arg(interface.kwaylandClientName());
}

void Generator::generateClientClassDtor(const Interface &interface)
{
    *m_stream.localData() << QStringLiteral("    virtual ~%1();\n\n").arg(interface.kwaylandClientName());
}

void Generator::generateClientClassReleaseDestroy(const Interface &interface)
{
    const QString templateString = QStringLiteral(
"    /**\n"
"     * @returns @c true if managing a %2.\n"
"     **/\n"
"    bool isValid() const;\n"
"    /**\n"
"     * Releases the %2 interface.\n"
"     * After the interface has been released the %1 instance is no\n"
"     * longer valid and can be setup with another %2 interface.\n"
"     **/\n"
"    void release();\n"
"    /**\n"
"     * Destroys the data held by this %1.\n"
"     * This method is supposed to be used when the connection to the Wayland\n"
"     * server goes away. If the connection is not valid anymore, it's not\n"
"     * possible to call release anymore as that calls into the Wayland\n"
"     * connection and the call would fail. This method cleans up the data, so\n"
"     * that the instance can be deleted or set up to a new %2 interface\n"
"     * once there is a new connection available.\n"
"     *\n"
"     * It is suggested to connect this method to ConnectionThread::connectionDied:\n"
"     * @code\n"
"     * connect(connection, &ConnectionThread::connectionDied, %3, &%1::destroy);\n"
"     * @endcode\n"
"     *\n"
"     * @see release\n"
"     **/\n"
"    void destroy();\n"
"\n");
    *m_stream.localData() << templateString.arg(interface.kwaylandClientName()).arg(interface.name()).arg(interface.kwaylandClientName().toLower());
}

void Generator::generateClientGlobalClassSetup(const Interface &interface)
{
    const QString templateString = QStringLiteral(
"    /**\n"
"     * Setup this %1 to manage the @p %3.\n"
"     * When using Registry::create%1 there is no need to call this\n"
"     * method.\n"
"     **/\n"
"    void setup(%2 *%3);\n");
    *m_stream.localData() << templateString.arg(interface.kwaylandClientName()).arg(interface.name()).arg(interface.kwaylandClientName().toLower());
}

void Generator::generateClientResourceClassSetup(const Interface &interface)
{
    const QString templateString = QStringLiteral(
"    /**\n"
"     * Setup this %1 to manage the @p %3.\n"
"     * When using %4::create%1 there is no need to call this\n"
"     * method.\n"
"     **/\n"
"    void setup(%2 *%3);\n");
    *m_stream.localData() << templateString.arg(interface.kwaylandClientName())
                                           .arg(interface.name())
                                           .arg(interface.kwaylandClientName().toLower())
                                           .arg(interface.factory()->kwaylandClientName());

}

void Generator::generateClientClassStart(const Interface &interface)
{
    const QString templateString = QStringLiteral(
"    /**\n"
"     * Sets the @p queue to use for creating objects with this %1.\n"
"     **/\n"
"    void setEventQueue(EventQueue *queue);\n"
"    /**\n"
"     * @returns The event queue to use for creating objects with this %1.\n"
"     **/\n"
"    EventQueue *eventQueue();\n\n");
    *m_stream.localData() << templateString.arg(interface.kwaylandClientName());
}

void Generator::generateClientClassRequests(const Interface &interface)
{
    const auto requests = interface.requests();
    const QString templateString = QStringLiteral("    void %1(%2);\n\n");
    const QString factoryTemplateString = QStringLiteral("    %1 *%2(%3);\n\n");
    for (const auto &r: requests) {
        if (r.isDestructor()) {
            continue;
        }
        QString arguments;
        bool first = true;
        QString factored;
        for (const auto &a: r.arguments()) {
            if (a.type() == Argument::Type::NewId) {
                factored = a.interface();
                continue;
            }
            if (!first) {
                arguments.append(QStringLiteral(", "));
            } else {
                first = false;
            }
            if (a.type() == Argument::Type::Object) {
                arguments.append(QStringLiteral("%1 *%2").arg(a.typeAsQt()).arg(toCamelCase(a.name())));
            } else {
                arguments.append(QStringLiteral("%1 %2").arg(a.typeAsQt()).arg(toCamelCase(a.name())));
            }
        }
        if (factored.isEmpty()) {
            *m_stream.localData() << templateString.arg(toCamelCase((r.name()))).arg(arguments);
        } else {
            if (!first) {
                arguments.append(QStringLiteral(", "));
            }
            arguments.append(QStringLiteral("QObject *parent = nullptr"));
            *m_stream.localData() << factoryTemplateString.arg(toQtInterfaceName(factored)).arg(toCamelCase(r.name())).arg(arguments);
        }
    }
}

void Generator::generateClientCppRequests(const Interface &interface)
{
    const auto requests = interface.requests();
    const QString templateString = QStringLiteral(
"void %1::%2(%3)\n"
"{\n"
"    Q_ASSERT(isValid());\n"
"    %4_%5(d->%6%7);\n"
"}\n\n");
    const QString factoryTemplateString = QStringLiteral(
"%2 *%1::%3(%4)\n"
"{\n"
"    Q_ASSERT(isValid());\n"
"    auto p = new %2(parent);\n"
"    auto w = %5_%6(d->%7%8);\n"
"    if (d->queue) {\n"
"        d->queue->addProxy(w);\n"
"    }\n"
"    p->setup(w);\n"
"    return p;\n"
"}\n\n"
    );
    for (const auto &r: requests) {
        if (r.isDestructor()) {
            continue;
        }
        QString arguments;
        QString requestArguments;
        bool first = true;
        QString factored;
        for (const auto &a: r.arguments()) {
            if (a.type() == Argument::Type::NewId) {
                factored = a.interface();
                continue;
            }
            if (!first) {
                arguments.append(QStringLiteral(", "));
            } else {
                first = false;
            }
            if (a.type() == Argument::Type::Object) {
                arguments.append(QStringLiteral("%1 *%2").arg(a.typeAsQt()).arg(toCamelCase(a.name())));
                requestArguments.append(QStringLiteral(", *%1").arg(toCamelCase(a.name())));
            } else {
                arguments.append(QStringLiteral("%1 %2").arg(a.typeAsQt()).arg(toCamelCase(a.name())));
                requestArguments.append(QStringLiteral(", %1").arg(toCamelCase(a.name())));
            }
        }
        if (factored.isEmpty()) {
            *m_stream.localData() << templateString.arg(interface.kwaylandClientName())
                                                   .arg(toCamelCase(r.name()))
                                                   .arg(arguments)
                                                   .arg(interface.name())
                                                   .arg(r.name())
                                                   .arg(interface.kwaylandClientName().toLower())
                                                   .arg(requestArguments);
        } else {
            if (!first) {
                arguments.append(QStringLiteral(", "));
            }
            arguments.append(QStringLiteral("QObject *parent"));
            *m_stream.localData() << factoryTemplateString.arg(interface.kwaylandClientName())
                                                          .arg(toQtInterfaceName(factored))
                                                          .arg(toCamelCase(r.name()))
                                                          .arg(arguments)
                                                          .arg(interface.name())
                                                          .arg(r.name())
                                                          .arg(interface.kwaylandClientName().toLower())
                                                          .arg(requestArguments);
        }
    }
}

void Generator::generateClientClassCasts(const Interface &interface)
{
    const QString templateString = QStringLiteral(
"    operator %1*();\n"
"    operator %1*() const;\n\n");
    *m_stream.localData() << templateString.arg(interface.name());
}

void Generator::generateClientGlobalClassEnd(const Interface &interface)
{
    Q_UNUSED(interface)
    *m_stream.localData() << QStringLiteral("private:\n");
    generateClientClassDptr(interface);
    *m_stream.localData() << QStringLiteral("};\n\n");
}

void Generator::generateClientClassDptr(const Interface &interface)
{
    Q_UNUSED(interface)
    *m_stream.localData() << QStringLiteral(
"    class Private;\n"
"    QScopedPointer<Private> d;\n");
}

void Generator::generateClientResourceClassEnd(const Interface &interface)
{
    *m_stream.localData() << QStringLiteral(
"private:\n"
"    friend class %2;\n"
"    explicit %1(QObject *parent = nullptr);\n"
    ).arg(interface.kwaylandClientName()).arg(interface.factory()->kwaylandClientName());
    generateClientClassDptr(interface);
    *m_stream.localData() << QStringLiteral("};\n\n");
}

void Generator::generateWaylandForwardDeclarations()
{
    for (auto it = m_interfaces.constBegin(); it != m_interfaces.constEnd(); ++it) {
        *m_stream.localData() << QStringLiteral("struct %1;\n").arg((*it).name());
    }
    *m_stream.localData() << "\n";
}

void Generator::generateNamespaceForwardDeclarations()
{
    QSet<QString> referencedObjects;
    for (auto it = m_interfaces.constBegin(); it != m_interfaces.constEnd(); ++it) {
        const auto events = (*it).events();
        const auto requests = (*it).requests();
        for (const auto &e: events) {
            const auto args = e.arguments();
            for (const auto &a: args) {
                if (a.type() != Argument::Type::Object && a.type() != Argument::Type::NewId) {
                    continue;
                }
                referencedObjects << a.interface();
            }
        }
        for (const auto &r: requests) {
            const auto args = r.arguments();
            for (const auto &a: args) {
                if (a.type() != Argument::Type::Object && a.type() != Argument::Type::NewId) {
                    continue;
                }
                referencedObjects << a.interface();
            }
        }
    }

    switch (m_project.localData()) {
    case Project::Client:
        *m_stream.localData() << QStringLiteral("class EventQueue;\n");
        for (const auto &o : referencedObjects) {
            auto it = s_clientClassNameMapping.constFind(o);
            if (it != s_clientClassNameMapping.constEnd()) {
                *m_stream.localData() << QStringLiteral("class %1;\n").arg(it.value());
            } else {
                qWarning() << "Cannot forward declare KWayland class for interface " << o;
            }
        }
        *m_stream.localData() << QStringLiteral("\n");
        break;
    case Project::Server:
        *m_stream.localData() << QStringLiteral("class Display;\n\n");
        break;
    default:
        Q_UNREACHABLE();
    }
}

void Generator::generateClientClassSignals(const Interface &interface)
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
    *m_stream.localData() << templateString.arg(interface.kwaylandClientName());
}

QString Generator::projectToName() const
{
    switch (m_project.localData()) {
    case Project::Client:
        return QStringLiteral("Client");
    case Project::Server:
        return QStringLiteral("Server");
    default:
        Q_UNREACHABLE();
    }
}

static void parseMapping()
{
    QFile mappingFile(QStringLiteral(MAPPING_FILE));
    mappingFile.open(QIODevice::ReadOnly);
    QTextStream stream(&mappingFile);
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        if (line.startsWith(QLatin1String("#")) || line.isEmpty()) {
            continue;
        }
        const QStringList parts = line.split(QStringLiteral(";"));
        if (parts.count() < 2) {
            continue;
        }
        s_clientClassNameMapping.insert(parts.first(), parts.at(1));
    }
}

}
}

int main(int argc, char **argv)
{
    using namespace KWayland::Tools;

    parseMapping();

    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    QCommandLineOption xmlFile(QStringList{QStringLiteral("x"), QStringLiteral("xml")},
                               QStringLiteral("The wayland protocol to parse."),
                               QStringLiteral("FileName"));
    QCommandLineOption fileName(QStringList{QStringLiteral("f"), QStringLiteral("file")},
                                QStringLiteral("The base name of files to be generated. E.g. for \"foo\" the files \"foo.h\" and \"foo.cpp\" are generated."
                                               "If not provided the base name gets derived from the xml protocol name"),
                                QStringLiteral("FileName"));

    parser.addHelpOption();
    parser.addOption(xmlFile);
    parser.addOption(fileName);

    parser.process(app);

    Generator generator(&app);
    generator.setXmlFileName(parser.value(xmlFile));
    generator.setBaseFileName(parser.value(fileName));
    generator.start();

    return app.exec();
}
