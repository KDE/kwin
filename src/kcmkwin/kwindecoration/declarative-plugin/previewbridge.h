/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once

#include <KDecoration2/DecorationButton>
#include <KDecoration2/Private/DecorationBridge>

#include <QList>
#include <QPointer>

class QQuickItem;

class KPluginFactory;

namespace KDecoration2
{
namespace Preview
{

class PreviewClient;
class PreviewItem;
class PreviewSettings;

class PreviewBridge : public DecorationBridge
{
    Q_OBJECT
    Q_PROPERTY(QString plugin READ plugin WRITE setPlugin NOTIFY pluginChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool valid READ isValid NOTIFY validChanged)
public:
    explicit PreviewBridge(QObject *parent = nullptr);
    ~PreviewBridge() override;
    std::unique_ptr<DecoratedClientPrivate> createClient(DecoratedClient *client, Decoration *decoration) override;
    std::unique_ptr<DecorationSettingsPrivate> settings(DecorationSettings *parent) override;

    PreviewClient *lastCreatedClient()
    {
        return m_lastCreatedClient;
    }
    PreviewSettings *lastCreatedSettings()
    {
        return m_lastCreatedSettings;
    }

    void registerPreviewItem(PreviewItem *item);
    void unregisterPreviewItem(PreviewItem *item);

    void setPlugin(const QString &plugin);
    QString plugin() const;
    void setTheme(const QString &theme);
    QString theme() const;
    bool isValid() const;

    KDecoration2::Decoration *createDecoration(QObject *parent = nullptr);
    KDecoration2::DecorationButton *createButton(KDecoration2::Decoration *decoration, KDecoration2::DecorationButtonType type, QObject *parent = nullptr);

public Q_SLOTS:
    void configure(QQuickItem *ctx);

Q_SIGNALS:
    void pluginChanged();
    void themeChanged();
    void validChanged();

private:
    void createFactory();
    void setValid(bool valid);
    PreviewClient *m_lastCreatedClient;
    PreviewSettings *m_lastCreatedSettings;
    QList<PreviewItem *> m_previewItems;
    QString m_plugin;
    QString m_theme;
    QPointer<KPluginFactory> m_factory;
    bool m_valid;
};

class BridgeItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString plugin READ plugin WRITE setPlugin NOTIFY pluginChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool valid READ isValid NOTIFY validChanged)
    Q_PROPERTY(KDecoration2::Preview::PreviewBridge *bridge READ bridge CONSTANT)

public:
    explicit BridgeItem(QObject *parent = nullptr);
    ~BridgeItem() override;

    void setPlugin(const QString &plugin)
    {
        m_bridge->setPlugin(plugin);
    }
    QString plugin() const
    {
        return m_bridge->plugin();
    }
    void setTheme(const QString &theme)
    {
        m_bridge->setTheme(theme);
    }
    QString theme() const
    {
        return m_bridge->theme();
    }
    bool isValid() const
    {
        return m_bridge->isValid();
    }

    PreviewBridge *bridge() const
    {
        return m_bridge;
    }

Q_SIGNALS:
    void pluginChanged();
    void themeChanged();
    void validChanged();

private:
    PreviewBridge *m_bridge;
};

}
}

Q_DECLARE_METATYPE(KDecoration2::Preview::PreviewBridge *)
