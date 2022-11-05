/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platform.h"

#include "dmabuftexture.h"
#include "inputbackend.h"
#include "openglbackend.h"
#include "output.h"
#include "outputconfiguration.h"
#include "qpainterbackend.h"

namespace KWin
{

Platform::Platform(QObject *parent)
    : QObject(parent)
    , m_eglDisplay(EGL_NO_DISPLAY)
{
}

Platform::~Platform()
{
}

std::unique_ptr<InputBackend> Platform::createInputBackend()
{
    return nullptr;
}

std::unique_ptr<OpenGLBackend> Platform::createOpenGLBackend()
{
    return nullptr;
}

std::unique_ptr<QPainterBackend> Platform::createQPainterBackend()
{
    return nullptr;
}

std::optional<DmaBufParams> Platform::testCreateDmaBuf(const QSize &size, quint32 format, const QVector<uint64_t> &modifiers)
{
    return {};
}

std::shared_ptr<DmaBufTexture> Platform::createDmaBufTexture(const QSize &size, quint32 format, uint64_t modifier)
{
    return {};
}

std::shared_ptr<DmaBufTexture> Platform::createDmaBufTexture(const DmaBufParams &attribs)
{
    return createDmaBufTexture({attribs.width, attribs.height}, attribs.format, attribs.modifier);
}

bool Platform::applyOutputChanges(const OutputConfiguration &config)
{
    const auto availableOutputs = outputs();
    QVector<Output *> toBeEnabledOutputs;
    QVector<Output *> toBeDisabledOutputs;
    for (const auto &output : availableOutputs) {
        if (config.constChangeSet(output)->enabled) {
            toBeEnabledOutputs << output;
        } else {
            toBeDisabledOutputs << output;
        }
    }
    for (const auto &output : toBeEnabledOutputs) {
        output->applyChanges(config);
    }
    for (const auto &output : toBeDisabledOutputs) {
        output->applyChanges(config);
    }
    return true;
}

Output *Platform::findOutput(const QString &name) const
{
    const auto candidates = outputs();
    for (Output *candidate : candidates) {
        if (candidate->name() == name) {
            return candidate;
        }
    }
    return nullptr;
}

void Platform::setReady(bool ready)
{
    if (m_ready == ready) {
        return;
    }
    m_ready = ready;
    Q_EMIT readyChanged(m_ready);
}

Output *Platform::createVirtualOutput(const QString &name, const QSize &size, double scale)
{
    return nullptr;
}

void Platform::removeVirtualOutput(Output *output)
{
    Q_ASSERT(!output);
}

EGLDisplay KWin::Platform::sceneEglDisplay() const
{
    return m_eglDisplay;
}

void Platform::setSceneEglDisplay(EGLDisplay display)
{
    m_eglDisplay = display;
}

QString Platform::supportInformation() const
{
    return QStringLiteral("Name: %1\n").arg(metaObject()->className());
}

EGLContext Platform::sceneEglGlobalShareContext() const
{
    return m_globalShareContext;
}

void Platform::setSceneEglGlobalShareContext(EGLContext context)
{
    m_globalShareContext = context;
}

} // namespace KWin
