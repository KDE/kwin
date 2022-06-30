/*
    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

namespace KWaylandServer
{
class Display;
class FractionalScaleManagerV1InterfacePrivate;

class KWIN_EXPORT FractionalScaleManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit FractionalScaleManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~FractionalScaleManagerV1Interface() override;

private:
    std::unique_ptr<FractionalScaleManagerV1InterfacePrivate> d;
};

} // namespace KWaylandServer
