/*
 *    SPDX-FileCopyrightText: 2026 Nicolas Fella <nicolas.fella@gmx.de>
 *
 *    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "a11ypointerlocator.h"

#include <QDBusConnection>

namespace KWin
{

A11yPointerLocator::A11yPointerLocator()
    : QObject()
{
    bool ret = QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/freedesktop/a11y/Manager"), this, QDBusConnection::ExportScriptableContents);
    qWarning() << "register it" << ret;
}

QVariantMap A11yPointerLocator::QueryPointer(double &rel_x, double &rel_y)
{
    sendErrorReply(QDBusError::ErrorType::Failed, "Bla");

    return {};
}

}
