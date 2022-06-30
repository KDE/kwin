/*
    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>

namespace KWaylandServer
{
class Display;
class FractionalScaleManagerV1InterfacePrivate;

/**
 * The ViewporterInterface is an extension that allows clients to crop and scale surfaces.
 *
 * The ViewporterInterface extensions provides a way for Wayland clients to crop and scale their
 * surfaces. This effectively breaks the direct connection between the buffer and the surface size.
 *
 * ViewporterInterface corresponds to the Wayland interface @c wp_viewporter.
 */
class KWIN_EXPORT FractionalScaleManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit FractionalScaleManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~FractionalScaleManagerV1Interface() override;

private:
    QScopedPointer<FractionalScaleManagerV1InterfacePrivate> d;
};

} // namespace KWaylandServer
