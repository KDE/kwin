/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_FB_BACKEND_H
#define KWIN_FB_BACKEND_H
#include "platform.h"

#include <QImage>
#include <QSize>

namespace KWin
{

class KWIN_EXPORT FramebufferBackend : public Platform
{
    Q_OBJECT
    Q_INTERFACES(KWin::Platform)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Platform" FILE "fbdev.json")
public:
    explicit FramebufferBackend(QObject *parent = nullptr);
    virtual ~FramebufferBackend();

    Screens *createScreens(QObject *parent = nullptr) override;
    QPainterBackend *createQPainterBackend() override;

    QSize screenSize() const override {
        return m_resolution;
    }

    void init() override;

    bool isValid() const {
        return m_fd >= 0;
    }

    QSize size() const {
        return m_resolution;
    }
    QSize physicalSize() const {
        return m_physicalSize;
    }

    void map();
    void unmap();
    void *mappedMemory() const {
        return m_memory;
    }
    int bytesPerLine() const {
        return m_bytesPerLine;
    }
    int bufferSize() const {
        return m_bufferLength;
    }
    quint32 bitsPerPixel() const {
        return m_bitsPerPixel;
    }
    QImage::Format imageFormat() const;
    /**
     * @returns whether the imageFormat is BGR instead of RGB.
     **/
    bool isBGR() const {
        return m_bgr;
    }

    QVector<CompositingType> supportedCompositors() const override {
        return QVector<CompositingType>{QPainterCompositing};
    }

private:
    void openFrameBuffer();
    bool queryScreenInfo();
    void initImageFormat();
    QSize m_resolution;
    QSize m_physicalSize;
    QByteArray m_id;
    struct Color {
        quint32 offset;
        quint32 length;
    };
    Color m_red;
    Color m_green;
    Color m_blue;
    Color m_alpha;
    quint32 m_bitsPerPixel = 0;
    int m_fd = -1;
    quint32 m_bufferLength = 0;
    int m_bytesPerLine = 0;
    void *m_memory = nullptr;
    QImage::Format m_imageFormat = QImage::Format_Invalid;
    bool m_bgr = false;
};

}

#endif
