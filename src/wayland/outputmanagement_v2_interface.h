/*
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

namespace KWaylandServer
{

class Display;
class OutputManagementV2InterfacePrivate;

/**
 * @class OutputManagementInterface
 *
 * This class is used to change the configuration of the Wayland server's outputs.
 * The client requests an OutputConfiguration, changes its OutputDevices and then
 * calls OutputConfiguration::apply, which makes this class emit a signal, carrying
 * the new configuration.
 * The server is then expected to make the requested changes by applying the settings
 * of the OutputDevices to the Outputs.
 *
 * @see OutputConfiguration
 * @see OutputConfigurationInterface
 */
class KWIN_EXPORT OutputManagementV2Interface : public QObject
{
    Q_OBJECT

public:
    explicit OutputManagementV2Interface(Display *display, QObject *parent = nullptr);
    ~OutputManagementV2Interface() override;

private:
    std::unique_ptr<OutputManagementV2InterfacePrivate> d;
};

}
