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
#ifndef KWIN_VIRTUAL_BACKEND_H
#define KWIN_VIRTUAL_BACKEND_H
#include "platform.h"

#include <kwin_export.h>

#include <QObject>
#include <QRect>

class QTemporaryDir;

struct gbm_device;

namespace KWin
{
class VirtualOutput;

class KWIN_EXPORT VirtualBackend : public Platform
{
    Q_OBJECT
    Q_INTERFACES(KWin::Platform)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Platform" FILE "virtual.json")

public:
    VirtualBackend(QObject *parent = nullptr);
    virtual ~VirtualBackend();
    void init() override;

    int outputCount() const {
        return m_outputs.size();
    }
    const QVector<VirtualOutput*> outputs() const {
        return m_outputs;
    }
    qreal outputScale() const {
        return m_outputScale;
    }

    bool saveFrames() const {
        return !m_screenshotDir.isNull();
    }
    QString screenshotDirPath() const;

    Screens *createScreens(QObject *parent = nullptr) override;
    QPainterBackend* createQPainterBackend() override;
    OpenGLBackend *createOpenGLBackend() override;

    Q_INVOKABLE void setVirtualOutputs(int count, QVector<QRect> geometries = QVector<QRect>());

    Q_INVOKABLE void setOutputScale(qreal scale) {
        m_outputScale = scale;
    }

    int drmFd() const {
        return m_drmFd;
    }
    void setDrmFd(int fd) {
        m_drmFd = fd;
    }

    gbm_device *gbmDevice() const {
        return m_gbmDevice;
    }
    void setGbmDevice(gbm_device *device) {
        m_gbmDevice = device;
    }
    virtual int gammaRampSize(int screen) const override;
    virtual bool setGammaRamp(int screen, ColorCorrect::GammaRamp &gamma) override;

    QVector<CompositingType> supportedCompositors() const override {
        return QVector<CompositingType>{OpenGLCompositing, QPainterCompositing};
    }

Q_SIGNALS:
    void virtualOutputsSet(bool countChanged);

private:
    QVector<VirtualOutput*> m_outputs;

    qreal m_outputScale = 1;

    QScopedPointer<QTemporaryDir> m_screenshotDir;
    int m_drmFd = -1;
    gbm_device *m_gbmDevice = nullptr;
};

}

#endif
