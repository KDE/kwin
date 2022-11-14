// SPDX-FileCopyrightText: 2022 David Redondo <david@david-redondo.de>
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#pragma once

#include <QDBusContext>
#include <QObject>

namespace KWin
{

class Output;

class NestedCompositorInterface : public QObject, QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.NestedCompositor")
public:
    Q_PROPERTY(int outputCount READ outputCount WRITE setOutputCount NOTIFY outputCountChanged);

    NestedCompositorInterface(QObject *parent = nullptr);

    int outputCount() const;
    void setOutputCount(int count);
public Q_SLOTS:
    void setOutputSize(int output, int width, int height);
Q_SIGNALS:
    void outputCountChanged();
    void outputCountRequested(int count);
    void resizeRequested(KWin::Output *output, const QSize &size);
};
}
