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
// system
#include <fcntl.h>
#include <unistd.h>
#include <config-kwin.h>

namespace KWin
{

VirtualInputDevice::VirtualInputDevice(QObject *parent)
    : InputDevice(parent)
{
}

void VirtualInputDevice::setPointer(bool set)
{
    m_pointer = set;
}

void VirtualInputDevice::setKeyboard(bool set)
{
    m_keyboard = set;
}

void VirtualInputDevice::setTouch(bool set)
{
    m_touch = set;
}

void VirtualInputDevice::setName(const QString &name)
{
    m_name = name;
}

QString VirtualInputDevice::sysName() const
{
    return QString();
}

QString VirtualInputDevice::name() const
{
    return m_name;
}

bool VirtualInputDevice::isEnabled() const
{
    return true;
}

void VirtualInputDevice::setEnabled(bool enabled)
{
    Q_UNUSED(enabled)
}

LEDs VirtualInputDevice::leds() const
{
    return LEDs();
}

void VirtualInputDevice::setLeds(LEDs leds)
{
    Q_UNUSED(leds)
}

bool VirtualInputDevice::isKeyboard() const
{
    return m_keyboard;
}

bool VirtualInputDevice::isAlphaNumericKeyboard() const
{
    return m_keyboard;
}

bool VirtualInputDevice::isPointer() const
{
    return m_pointer;
}

bool VirtualInputDevice::isTouchpad() const
{
    return false;
}

bool VirtualInputDevice::isTouch() const
{
    return m_touch;
}

bool VirtualInputDevice::isTabletTool() const
{
    return false;
}

bool VirtualInputDevice::isTabletPad() const
{
    return false;
}

bool VirtualInputDevice::isTabletModeSwitch() const
{
    return false;
}

bool VirtualInputDevice::isLidSwitch() const
{
    return false;
}

VirtualInputBackend::VirtualInputBackend(VirtualBackend *backend, QObject *parent)
    : InputBackend(parent)
    , m_backend(backend)
{
}

void VirtualInputBackend::initialize()
{
    Q_EMIT deviceAdded(m_backend->virtualPointer());
    Q_EMIT deviceAdded(m_backend->virtualKeyboard());
    Q_EMIT deviceAdded(m_backend->virtualTouch());
}

VirtualBackend::VirtualBackend(QObject *parent)
    : Platform(parent)
    , m_session(Session::create(Session::Type::Noop, this))
{
    m_virtualKeyboard.reset(new VirtualInputDevice());
    m_virtualKeyboard->setName(QStringLiteral("Virtual Keyboard 1"));
    m_virtualKeyboard->setKeyboard(true);

    m_virtualPointer.reset(new VirtualInputDevice());
    m_virtualPointer->setName(QStringLiteral("Virtual Pointer 1"));
    m_virtualPointer->setPointer(true);

    m_virtualTouch.reset(new VirtualInputDevice());
    m_virtualTouch->setName(QStringLiteral("Virtual Touch 1"));
    m_virtualTouch->setTouch(true);

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
        Q_EMIT outputAdded(dummyOutput);
        Q_EMIT outputEnabled(dummyOutput);
    }
    setReady(true);

    Q_EMIT screensQueried();
    return true;
}

VirtualInputDevice *VirtualBackend::virtualPointer() const
{
    return m_virtualPointer.data();
}

VirtualInputDevice *VirtualBackend::virtualKeyboard() const
{
    return m_virtualKeyboard.data();
}

VirtualInputDevice *VirtualBackend::virtualTouch() const
{
    return m_virtualTouch.data();
}

QString VirtualBackend::screenshotDirPath() const
{
    if (m_screenshotDir.isNull()) {
        return QString();
    }
    return m_screenshotDir->path();
}

InputBackend *VirtualBackend::createInputBackend()
{
    return new VirtualInputBackend(this);
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

    const QVector<VirtualOutput *> disabled = m_outputsEnabled;
    const QVector<VirtualOutput *> removed = m_outputs;

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
        Q_EMIT outputAdded(vo);
        Q_EMIT outputEnabled(vo);
    }

    for (VirtualOutput *output : disabled) {
        m_outputsEnabled.removeOne(output);
        Q_EMIT outputDisabled(output);
    }

    for (VirtualOutput *output : removed) {
        m_outputs.removeOne(output);
        Q_EMIT outputRemoved(output);
        delete output;
    }

    Q_EMIT screensQueried();
}

void VirtualBackend::enableOutput(VirtualOutput *output, bool enable)
{
    if (enable) {
        Q_ASSERT(!m_outputsEnabled.contains(output));
        m_outputsEnabled << output;
        Q_EMIT outputEnabled(output);
    } else {
        Q_ASSERT(m_outputsEnabled.contains(output));
        m_outputsEnabled.removeOne(output);
        Q_EMIT outputDisabled(output);
    }

    Q_EMIT screensQueried();
}

void VirtualBackend::removeOutput(AbstractOutput *output)
{
    VirtualOutput *virtualOutput = static_cast<VirtualOutput *>(output);
    virtualOutput->setEnabled(false);

    m_outputs.removeOne(virtualOutput);
    Q_EMIT outputRemoved(virtualOutput);

    delete virtualOutput;

    Q_EMIT screensQueried();
}

}
