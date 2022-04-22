/*
    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

namespace KWaylandServer
{
class Display;
class OutputDeviceV2Interface;
class PrimaryOutputV1InterfacePrivate;

class KWAYLANDSERVER_EXPORT PrimaryOutputV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit PrimaryOutputV1Interface(Display *display, QObject *parent = nullptr);
    ~PrimaryOutputV1Interface() override;

    /**
     * Sets a primary output's @p outputName for the current display configuration
     *
     * It's up to the compositor to decide what the semantics are for it.
     */
    void setPrimaryOutput(const QString &outputName);

private:
    QScopedPointer<PrimaryOutputV1InterfacePrivate> d;
};

}
