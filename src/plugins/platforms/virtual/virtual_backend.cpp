/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_backend.h"
#include "virtual_output.h"
#include "scene_qpainter_virtual_backend.h"
#include "session.h"
#include "wayland_server.h"
#include "egl_gbm_backend.h"
// Qt
#include <QTemporaryDir>
// KWayland
#include <KWaylandServer/seat_interface.h>
// system
#include <fcntl.h>
#include <unistd.h>
#include <config-kwin.h>

namespace KWin
{

VirtualBackend::VirtualBackend(QObject *parent)
    : Platform(parent)
    , m_session(Session::create(Session::Type::Noop, this))
{
    if (qEnvironmentVariableIsSet("KWIN_WAYLAND_VIRTUAL_SCREENSHOTS")) {
        m_screenshotDir.reset(new QTemporaryDir);
        if (!m_screenshotDir->isValid()) {
            m_screenshotDir.reset();
        }
        if (!m_screenshotDir.isNull()) {
            qDebug() << "Screenshots saved to: " << m_screenshotDir->path();
        }
    }

    supportsOutputChanges();
    setSupportsPointerWarping(true);
    setSupportsGammaControl(true);
    setPerScreenRenderingEnabled(true);
}

VirtualBackend::~VirtualBackend()
{
    if (sceneEglDisplay() != EGL_NO_DISPLAY) {
        eglTerminate(sceneEglDisplay());
    }
}

Session *VirtualBackend::session() const
{
    return m_session;
}

bool VirtualBackend::initialize()
{
    /*
     * Some tests currently expect one output present at start,
     * others set them explicitly.
     *
     * TODO: rewrite all tests to explicitly set the outputs.
     */
    if (m_outputs.isEmpty()) {
        VirtualOutput *dummyOutput = new VirtualOutput(this);
        dummyOutput->init(QPoint(0, 0), initialWindowSize());
        m_outputs << dummyOutput ;
        m_outputsEnabled << dummyOutput;
        emit outputAdded(dummyOutput);
        emit outputEnabled(dummyOutput);
    }

    setSoftwareCursorForced(true);
    setReady(true);
    waylandServer()->seat()->setHasPointer(true);
    waylandServer()->seat()->setHasKeyboard(true);
    waylandServer()->seat()->setHasTouch(true);

    emit screensQueried();
    return true;
}

QString VirtualBackend::screenshotDirPath() const
{
    if (m_screenshotDir.isNull()) {
        return QString();
    }
    return m_screenshotDir->path();
}

QPainterBackend *VirtualBackend::createQPainterBackend()
{
    return new VirtualQPainterBackend(this);
}

OpenGLBackend *VirtualBackend::createOpenGLBackend()
{
    return new EglGbmBackend(this);
}

Outputs VirtualBackend::outputs() const
{
    return m_outputs;
}

Outputs VirtualBackend::enabledOutputs() const
{
    return m_outputsEnabled;
}

void VirtualBackend::setVirtualOutputs(int count, QVector<QRect> geometries, QVector<int> scales)
{
    Q_ASSERT(geometries.size() == 0 || geometries.size() == count);
    Q_ASSERT(scales.size() == 0 || scales.size() == count);

    while (!m_outputsEnabled.isEmpty()) {
        VirtualOutput *output = m_outputsEnabled.takeLast();
        emit outputDisabled(output);
    }

    while (!m_outputs.isEmpty()) {
        VirtualOutput *output = m_outputs.takeLast();
        emit outputRemoved(output);
        delete output;
    }

    int sumWidth = 0;
    for (int i = 0; i < count; i++) {
        VirtualOutput *vo = new VirtualOutput(this);
        if (geometries.size()) {
            const QRect geo = geometries.at(i);
            vo->init(geo.topLeft(), geo.size());
        } else {
            vo->init(QPoint(sumWidth, 0), initialWindowSize());
            sumWidth += initialWindowSize().width();
        }
        if (scales.size()) {
            vo->setScale(scales.at(i));
        }
        m_outputs.append(vo);
        m_outputsEnabled.append(vo);
        emit outputAdded(vo);
        emit outputEnabled(vo);
    }

    emit screensQueried();
}

void VirtualBackend::enableOutput(VirtualOutput *output, bool enable)
{
    if (enable) {
        Q_ASSERT(!m_outputsEnabled.contains(output));
        m_outputsEnabled << output;
        emit outputEnabled(output);
    } else {
        Q_ASSERT(m_outputsEnabled.contains(output));
        m_outputsEnabled.removeOne(output);
        emit outputDisabled(output);
    }

    emit screensQueried();
}

void VirtualBackend::removeOutput(AbstractOutput *output)
{
    VirtualOutput *virtualOutput = static_cast<VirtualOutput *>(output);
    virtualOutput->setEnabled(false);

    m_outputs.removeOne(virtualOutput);
    emit outputRemoved(virtualOutput);

    delete virtualOutput;

    emit screensQueried();
}

}
