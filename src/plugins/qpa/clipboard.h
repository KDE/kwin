/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "wayland/abstract_data_source.h"

#include <QMimeData>

#include <qpa/qplatformclipboard.h>

namespace KWin::QPA
{

class ClipboardDataSource : public AbstractDataSource
{
    Q_OBJECT

public:
    explicit ClipboardDataSource(QMimeData *mimeData, QObject *parent = nullptr);

    QMimeData *mimeData() const;

    void requestData(const QString &mimeType, qint32 fd) override;
    void cancel() override;
    QStringList mimeTypes() const override;

private:
    QMimeData *m_mimeData;
};

class ClipboardMimeData : public QMimeData
{
    Q_OBJECT

public:
    explicit ClipboardMimeData(AbstractDataSource *dataSource);

protected:
    QVariant retrieveData(const QString &mimetype, QMetaType preferredType) const override;

private:
    AbstractDataSource *m_dataSource;
};

class Clipboard : public QObject, public QPlatformClipboard
{
    Q_OBJECT

public:
    Clipboard();

    void initialize();

    QMimeData *mimeData(QClipboard::Mode mode = QClipboard::Clipboard) override;
    void setMimeData(QMimeData *data, QClipboard::Mode mode = QClipboard::Clipboard) override;
    bool supportsMode(QClipboard::Mode mode) const override;
    bool ownsMode(QClipboard::Mode mode) const override;

private:
    QMimeData m_emptyData;
    std::unique_ptr<ClipboardMimeData> m_externalMimeData;
    std::unique_ptr<ClipboardDataSource> m_ownSelection;
};

}
