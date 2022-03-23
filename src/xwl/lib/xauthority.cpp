/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2021 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "xauthority.h"

#include <QDataStream>
#include <QHostInfo>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QTemporaryFile>

static void writeXauthorityEntry(QDataStream &stream, quint16 family,
                                 const QByteArray &address, const QByteArray &display,
                                 const QByteArray &name, const QByteArray &cookie)
{
    stream << quint16(family);

    auto writeArray = [&stream](const QByteArray &str) {
        stream << quint16(str.size());
        stream.writeRawData(str.constData(), str.size());
    };

    writeArray(address);
    writeArray(display);
    writeArray(name);
    writeArray(cookie);
}

static QByteArray generateXauthorityCookie()
{
    QByteArray cookie;
    cookie.resize(16); // Cookie must be 128bits

    QRandomGenerator *generator = QRandomGenerator::system();
    for (int i = 0; i < cookie.size(); ++i) {
        cookie[i] = uint8_t(generator->bounded(256));
    }
    return cookie;
}

bool generateXauthorityFile(int display, QTemporaryFile *authorityFile)
{
    const QString runtimeDirectory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);

    authorityFile->setFileTemplate(runtimeDirectory + QStringLiteral("/xauth_XXXXXX"));
    if (!authorityFile->open()) {
        return false;
    }

    const QByteArray hostname = QHostInfo::localHostName().toUtf8();
    const QByteArray displayName = QByteArray::number(display);
    const QByteArray name = QByteArrayLiteral("MIT-MAGIC-COOKIE-1");
    const QByteArray cookie = generateXauthorityCookie();

    QDataStream stream(authorityFile);
    stream.setByteOrder(QDataStream::BigEndian);

    // Write entry with FamilyLocal and the host name as address
    writeXauthorityEntry(stream, 256 /* FamilyLocal */, hostname, displayName, name, cookie);

    // Write entry with FamilyWild, no address
    writeXauthorityEntry(stream, 65535 /* FamilyWild */, QByteArray{}, displayName, name, cookie);

    if (stream.status() != QDataStream::Ok || !authorityFile->flush()) {
        authorityFile->remove();
        return false;
    }

    return true;
}
