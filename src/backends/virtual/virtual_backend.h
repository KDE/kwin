/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_VIRTUAL_BACKEND_H
#define KWIN_VIRTUAL_BACKEND_H

#include "inputbackend.h"
#include "inputdevice.h"
#include "platform.h"

#include <kwin_export.h>

#include <QObject>
#include <QRect>

class QTemporaryDir;

namespace KWin
{
class VirtualBackend;
class VirtualOutput;

class VirtualInputDevice : public InputDevice
{
    Q_OBJECT

public:
    explicit VirtualInputDevice(QObject *parent = nullptr);

    void setPointer(bool set);
    void setKeyboard(bool set);
    void setTouch(bool set);
    void setName(const QString &name);

    QString sysName() const override;
    QString name() const override;

    bool isEnabled() const override;
    void setEnabled(bool enabled) override;

    LEDs leds() const override;
    void setLeds(LEDs leds) override;

    bool isKeyboard() const override;
    bool isAlphaNumericKeyboard() const override;
    bool isPointer() const override;
    bool isTouchpad() const override;
    bool isTouch() const override;
    bool isTabletTool() const override;
    bool isTabletPad() const override;
    bool isTabletModeSwitch() const override;
    bool isLidSwitch() const override;

private:
    QString m_name;
    bool m_pointer = false;
    bool m_keyboard = false;
    bool m_touch = false;
};

class VirtualInputBackend : public InputBackend
{
    Q_OBJECT

public:
    explicit VirtualInputBackend(VirtualBackend *backend, QObject *parent = nullptr);

    void initialize() override;

private:
    VirtualBackend *m_backend;
};

class KWIN_EXPORT VirtualBackend : public Platform
{
    Q_OBJECT
    Q_INTERFACES(KWin::Platform)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Platform" FILE "virtual.json")

public:
    VirtualBackend(QObject *parent = nullptr);
    ~VirtualBackend() override;

    Session *session() const override;
    bool initialize() override;

    bool saveFrames() const {
        return !m_screenshotDir.isNull();
    }
    QString screenshotDirPath() const;

    VirtualInputDevice *virtualPointer() const;
    VirtualInputDevice *virtualKeyboard() const;
    VirtualInputDevice *virtualTouch() const;

    InputBackend *createInputBackend() override;
    QPainterBackend* createQPainterBackend() override;
    OpenGLBackend *createOpenGLBackend() override;

    Q_INVOKABLE void setVirtualOutputs(int count, QVector<QRect> geometries = QVector<QRect>(), QVector<int> scales = QVector<int>());

    Outputs outputs() const override;
    Outputs enabledOutputs() const override;

    QVector<CompositingType> supportedCompositors() const override {
        if (selectedCompositor() != NoCompositing) {
            return {selectedCompositor()};
        }
        return QVector<CompositingType>{OpenGLCompositing, QPainterCompositing};
    }

    void enableOutput(VirtualOutput *output, bool enable);

    Q_INVOKABLE void removeOutput(AbstractOutput *output);

Q_SIGNALS:
    void virtualOutputsSet(bool countChanged);

private:
    QVector<VirtualOutput*> m_outputs;
    QVector<VirtualOutput*> m_outputsEnabled;
    QScopedPointer<QTemporaryDir> m_screenshotDir;
    Session *m_session;

    QScopedPointer<VirtualInputDevice> m_virtualPointer;
    QScopedPointer<VirtualInputDevice> m_virtualKeyboard;
    QScopedPointer<VirtualInputDevice> m_virtualTouch;
};

}

#endif
