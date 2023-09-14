/*
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2022 Adrien Faveraux <ad1rie3@hotmail.fr>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QObject>

#include "qwayland-server-presentation-time.h"
#include "surface_interface.h"

// Qt
#include <QVector>
// std
#include <ctime>
#include <memory>

namespace KWaylandServer
{

class PresentationManagerInterfacePrivate;
class PresentationFeedbackInterfacePrivate;
class Display;

class PresentationManagerInterface : public QObject
{
    Q_OBJECT
public:
    explicit PresentationManagerInterface(Display *display, QObject *parent = nullptr);
    ~PresentationManagerInterface();

    clockid_t clockId() const;
    void setClockId(clockid_t clockId);

private:
    std::unique_ptr<PresentationManagerInterfacePrivate> d;
};

class PresentationFeedbackInterface : public QObject, private QtWaylandServer::wp_presentation_feedback
{
    Q_OBJECT
public:
    enum class Kind {
        Vsync = 1 << 0,
        HwClock = 1 << 1,
        HwCompletion = 1 << 2,
        ZeroCopy = 1 << 3,
    };
    Q_DECLARE_FLAGS(Kinds, Kind)
    PresentationFeedbackInterface(wl_client *client, uint32_t id);
    ~PresentationFeedbackInterface();

    void sync(OutputInterface *output);
    void presented(uint32_t tvSecHi,
                   uint32_t tvSecLo,
                   uint32_t tvNsec,
                   uint32_t refresh,
                   uint32_t seqHi,
                   uint32_t seqLo,
                   Kinds kinds);
    void discarded();

Q_SIGNALS:
    void resourceDestroyed();

protected:
    void wp_presentation_feedback_destroy_resource(Resource *resource) override;
};

class PresentationFeedbacks : public QObject
{
    Q_OBJECT
public:
    explicit PresentationFeedbacks(SurfaceInterface *surface, QObject *parent = nullptr);
    ~PresentationFeedbacks() override;

    bool isActive();
    void add(PresentationFeedbackInterface *feedback);
    void unsetOutput(const OutputInterface *output);
    void setOutput(OutputInterface *output);
    OutputInterface *output() const;
    void handleOutputRemoval();

public Q_SLOTS:
    void presented(std::chrono::nanoseconds current);
    void discard();
    void handleSurfaceDelete();

private:
    QVector<PresentationFeedbackInterface *> m_feedbacks;
    OutputInterface *m_output = nullptr;
    SurfaceInterface *m_surface;
    KWin::RenderLoop *m_loop = nullptr;
    KWin::RenderLoopPrivate *m_renderLoopPrivate = nullptr;
};

}

Q_DECLARE_METATYPE(KWaylandServer::PresentationFeedbackInterface *)
Q_DECLARE_OPERATORS_FOR_FLAGS(KWaylandServer::PresentationFeedbackInterface::Kinds)
