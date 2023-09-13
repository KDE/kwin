/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

namespace KWaylandServer
{
class Display;
class DataControlSourceV1Interface;
class DataControlDeviceManagerV1InterfacePrivate;
class DataControlDeviceV1Interface;

/**
 * The DataControlDeviceManagerV1Interface provides a way for privileged clients such as clipboard
 * managers to manage the current selection.
 *
 * DataControlDeviceManagerV1Interface corresponds to the Wayland interface @c zwlr_data_control_manager_v1.
 */
class KWIN_EXPORT DataControlDeviceManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit DataControlDeviceManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~DataControlDeviceManagerV1Interface() override;

Q_SIGNALS:
    void dataSourceCreated(KWaylandServer::DataControlSourceV1Interface *dataSource);
    void dataDeviceCreated(KWaylandServer::DataControlDeviceV1Interface *dataDevice);

private:
    std::unique_ptr<DataControlDeviceManagerV1InterfacePrivate> d;
};

}
