/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_backend.h"

#include "virtual_egl_backend.h"
#include "virtual_output.h"
#include "virtual_qpainter_backend.h"
// Qt
#include <QTemporaryDir>

namespace KWin
{

VirtualBackend::VirtualBackend(QObject *parent)
    : OutputBackend(parent)
{
    if (qEnvironmentVariableIsSet("KWIN_WAYLAND_VIRTUAL_SCREENSHOTS")) {
        m_screenshotDir.reset(new QTemporaryDir);
        if (!m_screenshotDir->isValid()) {
            m_screenshotDir.reset();
        }
        if (m_screenshotDir) {
            qDebug() << "Screenshots saved to: " << m_screenshotDir->path();
        }
    }
}

VirtualBackend::~VirtualBackend()
{
    if (sceneEglDisplay() != EGL_NO_DISPLAY) {
        eglTerminate(sceneEglDisplay());
    }
}

bool VirtualBackend::initialize()
{
    return true;
}

QString VirtualBackend::screenshotDirPath() const
{
    if (!m_screenshotDir) {
        return QString();
    }
    return m_screenshotDir->path();
}

std::unique_ptr<QPainterBackend> VirtualBackend::createQPainterBackend()
{
    return std::make_unique<VirtualQPainterBackend>(this);
}

std::unique_ptr<OpenGLBackend> VirtualBackend::createOpenGLBackend()
{
    return std::make_unique<VirtualEglBackend>(this);
}

Outputs VirtualBackend::outputs() const
{
    return m_outputs;
}

void VirtualBackend::setVirtualOutputs(const QVector<QRect> &geometries, QVector<int> scales)
{
    Q_ASSERT(scales.size() == 0 || scales.size() == geometries.size());

    const QVector<VirtualOutput *> removed = m_outputs;

    for (int i = 0; i < geometries.size(); i++) {
        VirtualOutput *vo = new VirtualOutput(this);
        vo->init(geometries[i].topLeft(), geometries[i].size());
        if (scales.size()) {
            vo->updateScale(scales.at(i));
        }
        m_outputs.append(vo);
        Q_EMIT outputAdded(vo);
        vo->updateEnabled(true);
    }

    for (VirtualOutput *output : removed) {
        output->updateEnabled(false);
        m_outputs.removeOne(output);
        Q_EMIT outputRemoved(output);
        output->unref();
    }

    Q_EMIT outputsQueried();
}

} // namespace KWin
