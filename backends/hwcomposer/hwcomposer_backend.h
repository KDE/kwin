/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_HWCOMPOSER_BACKEND_H
#define KWIN_HWCOMPOSER_BACKEND_H
#include "abstract_backend.h"

#include <QMutex>
#include <QWaitCondition>

// libhybris
#include <hwcomposer_window.h>
// needed as hwcomposer_window.h includes EGL which on non-arm includes Xlib
#include <fixx11h.h>

typedef struct hwc_display_contents_1 hwc_display_contents_1_t;
typedef struct hwc_layer_1 hwc_layer_1_t;
typedef struct hwc_composer_device_1 hwc_composer_device_1_t;

class HWComposerNativeWindowBuffer;

namespace KWin
{

class HwcomposerWindow;

class HwcomposerBackend : public AbstractBackend
{
    Q_OBJECT
    Q_INTERFACES(KWin::AbstractBackend)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.AbstractBackend" FILE "hwcomposer.json")
public:
    explicit HwcomposerBackend(QObject *parent = nullptr);
    virtual ~HwcomposerBackend();

    void init() override;
    Screens *createScreens(QObject *parent = nullptr) override;
    OpenGLBackend *createOpenGLBackend() override;

    HwcomposerWindow *createSurface();

    QSize size() const {
        return m_displaySize;
    }

    hwc_composer_device_1_t *device() const {
        return m_device;
    }
    int refreshRate() const {
        return m_refreshRate;
    }
    void enableVSync(bool enable);
    void waitVSync();
    void wakeVSync();

private Q_SLOTS:
    void toggleBlankOutput();

private:
    QSize m_displaySize;
    hwc_composer_device_1_t *m_device = nullptr;
    bool m_outputBlank = true;
    int m_refreshRate = 60000;
    int m_vsyncInterval = 16;
    bool m_hasVsync = false;
    QMutex m_vsyncMutex;
    QWaitCondition m_vsyncWaitCondition;
};

class HwcomposerWindow : public HWComposerNativeWindow
{
public:
    virtual ~HwcomposerWindow();

    void present(HWComposerNativeWindowBuffer *buffer);

private:
    friend HwcomposerBackend;
    HwcomposerWindow(HwcomposerBackend *backend);
    HwcomposerBackend *m_backend;
    hwc_display_contents_1_t **m_list;
};

}

#endif
