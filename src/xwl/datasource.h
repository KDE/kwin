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
Q_SIGNALS:
    void dataRequested(const QString &mimeType, qint32 fd);

private:
    QStringList m_mimeTypes;
};
}
}
