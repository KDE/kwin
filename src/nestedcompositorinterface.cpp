// <one line to give the program's name and a brief idea of what it does.>
// SPDX-FileCopyrightText: 2022 David Redondo <david@david-redondo.de>
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "nestedcompositorinterface.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "main.h"

#include <QDBusConnection>

namespace KWin
{
NestedCompositorInterface::NestedCompositorInterface(QObject *parent)
    : QObject(parent)
{
    connect(kwinApp()->outputBackend(), &OutputBackend::outputAdded, this, &NestedCompositorInterface::outputCountChanged);
    QDBusConnection::sessionBus().registerObject("/org/kde/KWin/NestedCompositor", this, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllProperties);
}

int NestedCompositorInterface::outputCount() const
{
    return kwinApp()->outputBackend()->outputs().size();
}

void NestedCompositorInterface::setOutputCount(int count)
{
    Q_EMIT outputCountRequested(count);
}

void NestedCompositorInterface::setOutputSize(int outputNumber, int width, int height)
{
    auto output = kwinApp()->outputBackend()->outputs().value(outputNumber);
    if (!output) {
        if (calledFromDBus()) {
            sendErrorReply(QDBusError::InvalidArgs, QStringLiteral("Invalid output number"));
        }
        return;
    }
    const QSize size(width, height);
    if (size.isEmpty()) {
        if (calledFromDBus()) {
            sendErrorReply(QDBusError::InvalidArgs, QStringLiteral("Invalid size"));
        }
        return;
    }
    Q_EMIT resizeRequested(output, size);
}

}
