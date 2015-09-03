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
#ifndef KWAYLAND_TOOLS_GENERATOR_H
#define KWAYLAND_TOOLS_GENERATOR_H

#include <QObject>
#include <QMutex>
#include <QThreadStorage>
#include <QWaitCondition>

class QTextStream;

namespace KWayland
{
namespace Tools
{

class Generator : public QObject
{
    Q_OBJECT
public:
    explicit Generator(QObject *parent = nullptr);
    virtual ~Generator();

    void setClassName(const QString &name) {
        m_className = name;
    }
    void setBaseFileName(const QString &name) {
        m_baseFileName = name;
    }
    void setWaylandGlobal(const QString &name) {
        m_waylandGlobal = name;
    }
    enum class Project {
        Client,
        Server
    };
    void setProject(Project project) {
        m_project = project;
    }
    void start();

private:
    void generateCopyrightHeader();
    void generateStartIncludeGuard();
    void generateEndIncludeGuard();
    void generateStartNamespace();
    void generateEndNamespace();
    void generateHeaderIncludes();
    void generateCppIncludes();
    void generatePrivateClass();
    void generateClientPrivateClass();
    void generateClientCpp();
    void generateClass();
    void generateClientClassStart();
    void generateClientClassCasts();
    void generateClientClassSignals();
    void generateClientClassEnd();
    void generateWaylandForwardDeclarations();
    void generateNamespaceForwardDeclarations();
    void startAuthorNameProcess();
    void startAuthorEmailProcess();
    void startGenerateHeaderFile();
    void startGenerateCppFile();

    void checkEnd();

    QString projectToName() const;

    QThreadStorage<QTextStream*> m_stream;
    QString m_className;
    QString m_waylandGlobal;
    Project m_project;
    QString m_authorName;
    QString m_authorEmail;
    QString m_baseFileName;

    QMutex m_mutex;
    QWaitCondition m_waitCondition;

    int m_finishedCounter = 0;
};

}
}

#endif
