/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include "abstract_data_source.h"
#include "primaryselectiondevicemanager_v1.h"

namespace KWin
{
class PrimarySelectionSourceV1InterfacePrivate;

/**
 * @brief Represents the Resource for the zwp_primary_selection_source_v1 interface.
 * Lifespan is mapped to the underlying object
 */
class KWIN_EXPORT PrimarySelectionSourceV1Interface : public AbstractDataSource
{
    Q_OBJECT
public:
    ~PrimarySelectionSourceV1Interface() override;

    void requestData(const QString &mimeType, qint32 fd) override;
    void cancel() override;

    QStringList mimeTypes() const override;

    static PrimarySelectionSourceV1Interface *get(wl_resource *native);
    wl_client *client() const override;

private:
    friend class PrimarySelectionDeviceManagerV1InterfacePrivate;
    explicit PrimarySelectionSourceV1Interface(::wl_resource *resource);

    std::unique_ptr<PrimarySelectionSourceV1InterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWin::PrimarySelectionSourceV1Interface *)
