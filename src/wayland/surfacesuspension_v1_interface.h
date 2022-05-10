/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>

namespace KWaylandServer
{

class Display;
class SurfaceSuspensionManagerV1InterfacePrivate;

class KWIN_EXPORT SurfaceSuspensionManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit SurfaceSuspensionManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~SurfaceSuspensionManagerV1Interface() override;

private:
    QScopedPointer<SurfaceSuspensionManagerV1InterfacePrivate> d;
};

} // namespace KWaylandServer
