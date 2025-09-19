/*
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
    SPDX-FileCopyrightText: 2025 David Redondo <kde@david-redondo.de>
*/

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>

#include <iostream>

int main(int argc, char **argv)
{
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"), QStringLiteral("queryWindowInfo"));
    const QDBusReply<QVariantMap> reply = QDBusConnection::sessionBus().call(message, QDBus::Block, INT_MAX);
    if (!reply.isValid()) {
        if (reply.error().name() == QLatin1StringView("org.kde.KWin.Error.UserCancel")) {
            return 0;
        }
        std::cerr << qPrintable(reply.error().name()) << ":" << qPrintable(reply.error().message()) << std::endl;
        return 1;
    }
    for (const auto &[key, value] : reply.value().asKeyValueRange()) {
        std::cout << qPrintable(key) << ": " << qPrintable(value.toString()) << '\n';
    }
    return 0;
}
