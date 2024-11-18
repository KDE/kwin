/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"
#include <KDecoration3/Private/DecorationBridge>
#include <QObject>

class KPluginFactory;
namespace KDecoration3
{
class DecorationSettings;
}

namespace KWin
{

class Window;

namespace Decoration
{

class KWIN_EXPORT DecorationBridge : public KDecoration3::DecorationBridge
{
    Q_OBJECT
public:
    explicit DecorationBridge();

    static bool hasPlugin();

    void init();
    KDecoration3::Decoration *createDecoration(Window *window);

    std::unique_ptr<KDecoration3::DecoratedWindowPrivate> createClient(KDecoration3::DecoratedWindow *client, KDecoration3::Decoration *decoration) override;
    std::unique_ptr<KDecoration3::DecorationSettingsPrivate> settings(KDecoration3::DecorationSettings *parent) override;

    QString recommendedBorderSize() const
    {
        return m_recommendedBorderSize;
    }

    bool showToolTips() const
    {
        return m_showToolTips;
    }

    void reconfigure();

    const std::shared_ptr<KDecoration3::DecorationSettings> &settings() const
    {
        return m_settings;
    }

    QString supportInformation() const;

Q_SIGNALS:
    void metaDataLoaded();

private:
    QString readPlugin();
    void loadMetaData(const QJsonObject &object);
    void findTheme(const QVariantMap &map);
    bool initPlugin();
    QString readTheme() const;
    void readDecorationOptions();
    std::unique_ptr<KPluginFactory> m_factory;
    bool m_showToolTips;
    QString m_recommendedBorderSize;
    QString m_plugin;
    QString m_defaultTheme;
    QString m_theme;
    std::shared_ptr<KDecoration3::DecorationSettings> m_settings;
    bool m_noPlugin;
};
} // Decoration
} // KWin
