/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"

#include <QByteArrayView>
#include <memory>
#include <stdint.h>

struct wl_surface;
struct wp_fifo_manager_v1;
struct wp_fifo_v1;
struct wl_registry;

namespace KWin::WaylandClient
{

class KWIN_EXPORT FifoV1
{
public:
    explicit FifoV1(wp_fifo_v1 *handle);
    ~FifoV1();

    void setBarrier();
    void waitBarrier();

private:
    wp_fifo_v1 *const m_handle;
};

class KWIN_EXPORT FifoManagerV1
{
public:
    static const QByteArrayView s_interfaceName;

    explicit FifoManagerV1(::wl_registry *registry, uint32_t name, int version);
    ~FifoManagerV1();

    std::unique_ptr<FifoV1> createFifo(::wl_surface *surface);

private:
    wp_fifo_manager_v1 *const m_handle;
};

}
