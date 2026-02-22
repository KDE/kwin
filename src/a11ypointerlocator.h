/*
 *    SPDX-FileCopyrightText: 2026 Nicolas Fella <nicolas.fella@gmx.de>
 *
 *    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QDBusContext>
#include <QObject>

namespace KWin
{

class A11yPointerLocator : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.a11y.PointerLocator")
public:
    explicit A11yPointerLocator();

    Q_SCRIPTABLE QVariantMap QueryPointer(double &rel_x, double &rel_y);

Q_SIGNALS:
    void PointerPositionChanged();

private:
};
}
