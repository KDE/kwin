/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>

namespace KWaylandServer
{
class Display;
class PrimarySelectionSourceV1Interface;
class PrimarySelectionDeviceManagerV1InterfacePrivate;
class PrimarySelectionDeviceV1Interface;

/**
 * @brief Represents the Global for zwp_primary_selection_manager_v1 interface.
 *
 */
class KWIN_EXPORT PrimarySelectionDeviceManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit PrimarySelectionDeviceManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~PrimarySelectionDeviceManagerV1Interface();

Q_SIGNALS:
    void dataSourceCreated(KWaylandServer::PrimarySelectionSourceV1Interface *dataSource);
    void dataDeviceCreated(KWaylandServer::PrimarySelectionDeviceV1Interface *dataDevice);

private:
    QScopedPointer<PrimarySelectionDeviceManagerV1InterfacePrivate> d;
};

}
