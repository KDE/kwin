/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "clientbuffer.h"

namespace KWin
{
class ClientBufferRef;
class Toplevel;

class KWIN_EXPORT ClientBufferX11 : public ClientBuffer
{
    Q_OBJECT

public:
    explicit ClientBufferX11(const Toplevel *window);
    ~ClientBufferX11() override;

    xcb_pixmap_t pixmap() const;
    xcb_visualid_t visual() const;

    QSize size() const override;
    bool hasAlphaChannel() const override;

    static ClientBufferX11 *from(const ClientBufferRef &ref);

protected:
    void release() override;

private:
    xcb_visualid_t m_visual = XCB_NONE;
    xcb_pixmap_t m_pixmap = XCB_PIXMAP_NONE;
    QSize m_size;
    bool m_hasAlphaChannel = false;
};

} // namespace KWin
