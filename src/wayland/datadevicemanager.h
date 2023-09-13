/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

#include "datadevice.h"

namespace KWin
{
class Display;
class DataSourceInterface;
class DataDeviceManagerInterfacePrivate;

/**
 * @brief Represents the Global for wl_data_device_manager interface.
 *
 */
class KWIN_EXPORT DataDeviceManagerInterface : public QObject
{
    Q_OBJECT

public:
    explicit DataDeviceManagerInterface(Display *display, QObject *parent = nullptr);
    ~DataDeviceManagerInterface() override;

    /**
     * Drag and Drop actions supported by the DataSourceInterface.
     */
    enum class DnDAction {
        None = 0,
        Copy = 1 << 0,
        Move = 1 << 1,
        Ask = 1 << 2,
    };
    Q_DECLARE_FLAGS(DnDActions, DnDAction)

Q_SIGNALS:
    void dataSourceCreated(KWin::DataSourceInterface *);
    void dataDeviceCreated(KWin::DataDeviceInterface *);

private:
    std::unique_ptr<DataDeviceManagerInterfacePrivate> d;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::DataDeviceManagerInterface::DnDActions)
