/*
    SPDX-FileCopyrightText: 2024 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

namespace KWin
{
class Display;
class SurfaceInvalidtionManagerV1InterfacePrivate;

/**
 * The SurfaceInvalidationV1 is an extension that notifies clients when they need
 * to forcefully resend buffers
 *
 */
class KWIN_EXPORT SurfaceInvalidationManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit SurfaceInvalidationManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~SurfaceInvalidationManagerV1Interface() override;

    /**
     * Notify all clients watching that the surfaces are invalidated
     */
    void invalidateSurfaces();

private:
    std::unique_ptr<SurfaceInvalidtionManagerV1InterfacePrivate> d;
};

} // namespace KWin
