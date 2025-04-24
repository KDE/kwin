/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

struct wl_resource;

namespace KWin
{
class Display;
class BlurManagerInterfacePrivate;
class BlurInterface;
class SurfaceInterface;

/**
 * @brief Represents the Global for org_kde_kwin_blur_manager interface.
 *
 * This class creates BlurInterfaces and attaches them to SurfaceInterfaces.
 *
 * @see BlurInterface
 * @see SurfaceInterface
 */
class KWIN_EXPORT BlurManagerInterface : public QObject
{
    Q_OBJECT
public:
    explicit BlurManagerInterface(Display *display, QObject *parent = nullptr);
    ~BlurManagerInterface() override;

    void remove();

private:
    std::unique_ptr<BlurManagerInterfacePrivate> d;
};
}
