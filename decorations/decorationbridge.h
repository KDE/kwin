/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_DECORATION_BRIDGE_H
#define KWIN_DECORATION_BRIDGE_H

#include <kwinglobals.h>

#include <KDecoration2/Private/DecorationBridge>

#include <KSharedConfig>

#include <QObject>
#include <QSharedPointer>

class KPluginFactory;

namespace KDecoration2
{
class DecorationSettings;
}

namespace KWin
{

class AbstractClient;

namespace Decoration
{

class KWIN_EXPORT DecorationBridge : public KDecoration2::DecorationBridge
{
    Q_OBJECT
public:
    virtual ~DecorationBridge();

    void init();
    KDecoration2::Decoration *createDecoration(AbstractClient *client);

    std::unique_ptr<KDecoration2::DecoratedClientPrivate> createClient(KDecoration2::DecoratedClient *client, KDecoration2::Decoration *decoration) override;
    std::unique_ptr<KDecoration2::DecorationSettingsPrivate> settings(KDecoration2::DecorationSettings *parent) override;
    void update(KDecoration2::Decoration *decoration, const QRect &geometry) override;

    bool needsBlur() const {
        return m_blur;
    }
    QString recommendedBorderSize() const {
        return m_recommendedBorderSize;
    }

    bool showToolTips() const {
        return m_showToolTips;
    }

    void reconfigure();

    const QSharedPointer<KDecoration2::DecorationSettings> &settings() const {
        return m_settings;
    }

    QString supportInformation() const;

Q_SIGNALS:
    void metaDataLoaded();

private:
    QString readPlugin();
    void loadMetaData(const QJsonObject &object);
    void findTheme(const QVariantMap &map);
    void initPlugin();
    QString readTheme() const;
    void readDecorationOptions();
    KPluginFactory *m_factory;
    KSharedConfig::Ptr m_lnfConfig;
    bool m_blur;
    bool m_showToolTips;
    QString m_recommendedBorderSize;
    QString m_plugin;
    QString m_defaultTheme;
    QString m_theme;
    QSharedPointer<KDecoration2::DecorationSettings> m_settings;
    bool m_noPlugin;
    KWIN_SINGLETON(DecorationBridge)
};
} // Decoration
} // KWin

#endif
