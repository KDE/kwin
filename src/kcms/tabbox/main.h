/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Cyril Rossi <cyril.rossi@enioka.com>
    SPDX-FileCopyrightText: 2023 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QStandardItemModel>

#include <kcmodule.h>
#include <ksharedconfig.h>

namespace KWin
{
class KWinTabBoxConfigForm;
namespace TabBox
{
class KWinTabboxData;
class TabBoxSettings;
}

class KWinTabBoxConfig : public KCModule
{
    Q_OBJECT

public:
    explicit KWinTabBoxConfig(QObject *parent, const KPluginMetaData &data);
    ~KWinTabBoxConfig() override;

public Q_SLOTS:
    void save() override;
    void load() override;
    void defaults() override;

private Q_SLOTS:
    void updateUnmanagedState();
    void configureEffectClicked();

private:
    void initLayoutLists();
    void createConnections(KWinTabBoxConfigForm *form);

private:
    KWinTabBoxConfigForm *m_primaryTabBoxUi = nullptr;
    KWinTabBoxConfigForm *m_alternativeTabBoxUi = nullptr;
    std::unique_ptr<QStandardItemModel> m_switcherModel;
    KSharedConfigPtr m_config;

    TabBox::KWinTabboxData *m_data;
};

} // namespace
