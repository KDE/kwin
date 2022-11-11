/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "outputbackend.h"

#include "dmabuftexture.h"
#include "inputbackend.h"
#include "openglbackend.h"
#include "output.h"
#include "outputconfiguration.h"
#include "qpainterbackend.h"

namespace KWin
{

OutputBackend::OutputBackend(QObject *parent)
    : QObject(parent)
    , m_eglDisplay(EGL_NO_DISPLAY)
{
}

OutputBackend::~OutputBackend()
{
}

std::unique_ptr<InputBackend> OutputBackend::createInputBackend()
{
    return nullptr;
}

std::unique_ptr<OpenGLBackend> OutputBackend::createOpenGLBackend()
{
    return nullptr;
}

std::unique_ptr<QPainterBackend> OutputBackend::createQPainterBackend()
{
    return nullptr;
}

std::optional<DmaBufParams> OutputBackend::testCreateDmaBuf(const QSize &size, quint32 format, const QVector<uint64_t> &modifiers)
{
    return {};
}

std::shared_ptr<DmaBufTexture> OutputBackend::createDmaBufTexture(const QSize &size, quint32 format, uint64_t modifier)
{
    return {};
}

std::shared_ptr<DmaBufTexture> OutputBackend::createDmaBufTexture(const DmaBufParams &attribs)
{
    return createDmaBufTexture({attribs.width, attribs.height}, attribs.format, attribs.modifier);
}

bool OutputBackend::applyOutputChanges(const OutputConfiguration &config)
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

Output *OutputBackend::findOutput(const QString &name) const
{
    const auto candidates = outputs();
    for (Output *candidate : candidates) {
        if (candidate->name() == name) {
            return candidate;
        }
    }
    return nullptr;
}

Output *OutputBackend::createVirtualOutput(const QString &name, const QSize &size, double scale)
{
    return nullptr;
}

void OutputBackend::removeVirtualOutput(Output *output)
{
    Q_ASSERT(!output);
}

EGLDisplay KWin::OutputBackend::sceneEglDisplay() const
{
    return m_eglDisplay;
}

void OutputBackend::setSceneEglDisplay(EGLDisplay display)
{
    m_eglDisplay = display;
}

QString OutputBackend::supportInformation() const
{
    return QStringLiteral("Name: %1\n").arg(metaObject()->className());
}

EGLContext OutputBackend::sceneEglGlobalShareContext() const
{
    return m_globalShareContext;
}

void OutputBackend::setSceneEglGlobalShareContext(EGLContext context)
{
    m_globalShareContext = context;
}

} // namespace KWin
