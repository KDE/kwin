/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <chrono>
#include <memory>
#include <wayland-server-core.h>

struct wl_resource;

namespace KWin
{

class OutputInterface;
class SurfaceInterface;
class Display;
class PresentationInterfacePrivate;

class KWIN_EXPORT PresentationFeedback : public QObject
{
    Q_OBJECT

public:
    explicit PresentationFeedback(SurfaceInterface *surface);
    ~PresentationFeedback() override;

    void sendDiscarded();
    void sendPresented(std::chrono::nanoseconds timestamp, std::chrono::nanoseconds refresh, uint64_t sequence, uint32_t flags);
    void schedulePresented(OutputInterface *output);

private:
    SurfaceInterface *m_surface;
    OutputInterface *m_output = nullptr;
    wl_list m_resources;

    friend class PresentationInterfacePrivate;
};

class KWIN_EXPORT PresentationInterface : public QObject
{
    Q_OBJECT

public:
    explicit PresentationInterface(Display *display, QObject *parent = nullptr);
    ~PresentationInterface() override;

    clock_t clockId() const;
    void setClockId(clock_t clockId);

    PresentationFeedback *surfaceScannedOut(SurfaceInterface *surface, OutputInterface *output);
    PresentationFeedback *surfaceCopied(SurfaceInterface *surface, OutputInterface *output);

private:
    std::unique_ptr<PresentationInterfacePrivate> d;
};

} // namespace KWin
