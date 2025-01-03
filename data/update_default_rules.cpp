/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2005 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// read additional window rules and add them to kwinrulesrc

#include "config-kwin.h"

#include <KConfig>
#include <KConfigGroup>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <QStandardPaths>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        return 1;
    }

    QCoreApplication::setApplicationName("kwin_update_default_rules");

    QString file = QStandardPaths::locate(QStandardPaths::GenericDataLocation, KWIN_DATADIR + QString("/default_rules/%1").arg(argv[1]));
    if (file.isEmpty()) {
        qWarning() << "File " << argv[1] << " not found!";
        return 1;
    }
    KConfig src_cfg(file);
    KConfig dest_cfg(QStringLiteral("kwinrulesrc"), KConfig::NoGlobals);
    KConfigGroup scg(&src_cfg, QStringLiteral("General"));
    KConfigGroup dcg(&dest_cfg, QStringLiteral("General"));
    int count = scg.readEntry("count", 0);
    int pos = dcg.readEntry("count", 0);
    for (int group = 1;
         group <= count;
         ++group) {
        QMap<QString, QString> entries = src_cfg.entryMap(QString::number(group));
        ++pos;
        dest_cfg.deleteGroup(QString::number(pos));
        KConfigGroup dcg2(&dest_cfg, QString::number(pos));
        for (QMap<QString, QString>::ConstIterator it = entries.constBegin();
             it != entries.constEnd();
             ++it) {
            dcg2.writeEntry(it.key(), *it);
        }
    }
    dcg.writeEntry("count", pos);
    scg.sync();
    dcg.sync();
    // Send signal to all kwin instances
    QDBusMessage message =
        QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
}
