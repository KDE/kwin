/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platform.h"

#include "composite.h"
#include "cursor.h"
#include "dmabuftexture.h"
#include "effects.h"
#include "inputbackend.h"
#include "openglbackend.h"
#include "outline.h"
#include "output.h"
#include "outputconfiguration.h"
#include "overlaywindow.h"
#include "pointer_input.h"
#include "qpainterbackend.h"
#include "scene.h"
#include "screenedge.h"

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

PlatformCursorImage Platform::cursorImage() const
{
    Cursor *cursor = Cursors::self()->currentCursor();
    return PlatformCursorImage(cursor->image(), cursor->hotspot());
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
    Q_UNUSED(size)
    Q_UNUSED(format)
    Q_UNUSED(modifiers)
    return {};
}

std::shared_ptr<DmaBufTexture> Platform::createDmaBufTexture(const QSize &size, quint32 format, uint64_t modifier)
{
    Q_UNUSED(size)
    Q_UNUSED(format)
    Q_UNUSED(modifier)
    return {};
}

std::shared_ptr<DmaBufTexture> Platform::createDmaBufTexture(const DmaBufParams &attribs)
{
    return createDmaBufTexture({attribs.width, attribs.height}, attribs.format, attribs.modifier);
}

std::unique_ptr<Edge> Platform::createScreenEdge(ScreenEdges *edges)
{
    return std::make_unique<Edge>(edges);
}

void Platform::createPlatformCursor(QObject *parent)
{
    new InputRedirectionCursor(parent);
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
    Q_UNUSED(name);
    Q_UNUSED(size);
    Q_UNUSED(scale);
    return nullptr;
}

void Platform::removeVirtualOutput(Output *output)
{
    Q_ASSERT(!output);
}

void Platform::warpPointer(const QPointF &globalPos)
{
    Q_UNUSED(globalPos)
}

bool Platform::supportsNativeFence() const
{
    if (Compositor *compositor = Compositor::self()) {
        return compositor->scene()->supportsNativeFence();
    }
    return false;
}

EGLDisplay KWin::Platform::sceneEglDisplay() const
{
    return m_eglDisplay;
}

void Platform::setSceneEglDisplay(EGLDisplay display)
{
    m_eglDisplay = display;
}

bool Platform::requiresCompositing() const
{
    return true;
}

bool Platform::compositingPossible() const
{
    return true;
}

QString Platform::compositingNotPossibleReason() const
{
    return QString();
}

bool Platform::openGLCompositingIsBroken() const
{
    return false;
}

void Platform::createOpenGLSafePoint(OpenGLSafePoint safePoint)
{
    Q_UNUSED(safePoint)
}

void Platform::startInteractiveWindowSelection(std::function<void(KWin::Window *)> callback, const QByteArray &cursorName)
{
    if (!input()) {
        callback(nullptr);
        return;
    }
    input()->startInteractiveWindowSelection(callback, cursorName);
}

void Platform::startInteractivePositionSelection(std::function<void(const QPoint &)> callback)
{
    if (!input()) {
        callback(QPoint(-1, -1));
        return;
    }
    input()->startInteractivePositionSelection(callback);
}

std::unique_ptr<OverlayWindow> Platform::createOverlayWindow()
{
    return nullptr;
}

std::unique_ptr<OutlineVisual> Platform::createOutline(Outline *outline)
{
    if (Compositor::compositing()) {
        return std::make_unique<CompositedOutlineVisual>(outline);
    }
    return nullptr;
}

void Platform::createEffectsHandler(Compositor *compositor, Scene *scene)
{
    new EffectsHandlerImpl(compositor, scene);
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
