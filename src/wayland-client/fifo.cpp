/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "fifo.h"
#include "wayland-fifo-v1-client-protocol.h"

namespace KWin::WaylandClient
{

FifoV1::FifoV1(wp_fifo_v1 *handle)
    : m_handle(handle)
{
}

FifoV1::~FifoV1()
{
    wp_fifo_v1_destroy(m_handle);
}

void FifoV1::setBarrier()
{
    wp_fifo_v1_set_barrier(m_handle);
}

void FifoV1::waitBarrier()
{
    wp_fifo_v1_wait_barrier(m_handle);
}

const QByteArrayView FifoManagerV1::s_interfaceName = wp_fifo_manager_v1_interface.name;

FifoManagerV1::FifoManagerV1(::wl_registry *registry, uint32_t name, int version)
    : m_handle(reinterpret_cast<wp_fifo_manager_v1 *>(wl_registry_bind(registry, name, &wp_fifo_manager_v1_interface, version)))
{
}

FifoManagerV1::~FifoManagerV1()
{
    wp_fifo_manager_v1_destroy(m_handle);
}

std::unique_ptr<FifoV1> FifoManagerV1::createFifo(::wl_surface *surface)
{
    return std::make_unique<FifoV1>(wp_fifo_manager_v1_get_fifo(m_handle, surface));
}

}
