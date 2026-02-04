/*
    SPDX-FileCopyrightText: 2026 KWin

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <QPointer>
#include <memory>

class wl_resource;

namespace KWin
{

class Display;
class SeatInterface;
class SurfaceInterface;
class SurfaceRole;

class XXInputMethodManagerV2InterfacePrivate;
class XXInputMethodV1InterfacePrivate;
class XXInputPopupSurfaceV2InterfacePrivate;
class XXInputPopupPositionerV1InterfacePrivate;

/**
 * Wrapper for xx_input_method_manager_v2
 */
class KWIN_EXPORT XXInputMethodManagerV2Interface : public QObject
{
    Q_OBJECT
public:
    explicit XXInputMethodManagerV2Interface(Display *display, QObject *parent = nullptr);
    ~XXInputMethodManagerV2Interface() override;

Q_SIGNALS:
    void inputMethodCreated(KWin::XXInputMethodV1Interface *im);

private:
    std::unique_ptr<XXInputMethodManagerV2InterfacePrivate> d;
};

/**
 * Wrapper for xx_input_popup_surface_v2
 */
class KWIN_EXPORT XXInputPopupSurfaceV2Interface : public QObject
{
    Q_OBJECT
public:
    ~XXInputPopupSurfaceV2Interface() override;

    static SurfaceRole *role();
    SurfaceInterface *surface() const;

    // Send a configure sequence start with geometry and anchor geometry, returns serial
    quint32 sendStartConfigure(uint32_t width, uint32_t height,
                               int32_t anchorX, int32_t anchorY,
                               uint32_t anchorW, uint32_t anchorH);

Q_SIGNALS:
    void aboutToBeDestroyed();
    void ackConfigureReceived(quint32 serial);

private:
    friend class XXInputMethodV1InterfacePrivate;
    explicit XXInputPopupSurfaceV2Interface(SurfaceInterface *surface, wl_resource *resource, QObject *parent = nullptr);
    std::unique_ptr<XXInputPopupSurfaceV2InterfacePrivate> d;
};

/**
 * Wrapper for xx_input_method_v1 (from input_method_experimental_v2 protocol)
 */
class KWIN_EXPORT XXInputMethodV1Interface : public QObject
{
    Q_OBJECT
public:
    explicit XXInputMethodV1Interface(Display *display, SeatInterface *seat, wl_resource *resource);
    ~XXInputMethodV1Interface() override;

    // compositor -> client
    void sendActivate();
    void sendDeactivate();
    void sendSurroundingText(const QString &text, quint32 cursor, quint32 anchor);
    void sendContentType(uint32_t hint, uint32_t purpose);
    void sendDone();

Q_SIGNALS:
    // client -> compositor
    void commitString(const QString &text);
    void setPreeditString(const QString &text, qint32 cursorBegin, qint32 cursorEnd);
    void deleteSurroundingText(quint32 before, quint32 after);
    void commit(quint32 serial);
    void inputPopupSurfaceAdded(KWin::XXInputPopupSurfaceV2Interface *popup);

private:
    friend class XXInputMethodManagerV2InterfacePrivate;
    std::unique_ptr<XXInputMethodV1InterfacePrivate> d;
};

} // namespace KWin
