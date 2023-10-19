/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <QObject>
#include <chrono>

#include "core/renderbackend.h"
#include "libkwineffects/kwinglobals.h"
#include "qwayland-server-presentation-time.h"

namespace KWin
{

class Display;
class SurfaceInterface;

class PresentationTime : public QObject, QtWaylandServer::wp_presentation
{
    Q_OBJECT
public:
    explicit PresentationTime(Display *display, QObject *parent);

private:
    void wp_presentation_bind_resource(Resource *resource) override;
    void wp_presentation_destroy(Resource *resource) override;
    void wp_presentation_feedback(Resource *resource, wl_resource *surface, uint32_t callback) override;
};

class PresentationTimeFeedback : public PresentationFeedback, public QtWaylandServer::wp_presentation_feedback
{
public:
    explicit PresentationTimeFeedback(SurfaceInterface *surface, wl_client *client, uint32_t id);
    ~PresentationTimeFeedback() override;

    void presented(std::chrono::nanoseconds refreshCycleDuration, std::chrono::nanoseconds timestamp, PresentationMode mode) override;

private:
    void wp_presentation_feedback_destroy_resource(Resource *resource) override;

    SurfaceInterface *const m_surface;
    bool m_destroyed = false;
};

}
