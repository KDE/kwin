/*
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <KWaylandServer/abstract_data_source.h>

namespace KWin
{
namespace Xwl
{
class XwlDataSource : public KWaylandServer::AbstractDataSource
{
    Q_OBJECT
public:
    void requestData(const QString &mimeType, qint32 fd) override;
    void cancel() override;
    QStringList mimeTypes() const override;
    void setMimeTypes(const QStringList &mimeTypes);

    void accept(const QString &mimeType) override;
    KWaylandServer::DataDeviceManagerInterface::DnDActions supportedDragAndDropActions() const override;
    void setSupportedDndActions(KWaylandServer::DataDeviceManagerInterface::DnDActions dndActions);

    void dndAction(KWaylandServer::DataDeviceManagerInterface::DnDAction action) override;

    KWaylandServer::DataDeviceManagerInterface::DnDAction selectedDragAndDropAction() {
        return m_dndAction;
    }

    void dropPerformed() override {
        Q_EMIT dropped();
    }
    void dndFinished() override {
        Q_EMIT finished();
    }
    bool isAccepted() const override;

Q_SIGNALS:
    void dataRequested(const QString &mimeType, qint32 fd);
    void dropped();
    void finished();

private:
    QStringList m_mimeTypes;
    KWaylandServer::DataDeviceManagerInterface::DnDActions m_supportedDndActions;
    KWaylandServer::DataDeviceManagerInterface::DnDAction m_dndAction = KWaylandServer::DataDeviceManagerInterface::DnDAction::None;
    bool m_accepted = false;
};
}
}
