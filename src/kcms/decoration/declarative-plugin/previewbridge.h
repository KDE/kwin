/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once

#include <KDecoration3/DecorationButton>
#include <KDecoration3/Private/DecorationBridge>

#include <QList>
#include <QPointer>
#include <QQmlEngine>

class QQuickItem;

class KPluginFactory;

namespace KDecoration3
{
namespace Preview
{

class PreviewClient;
class PreviewItem;
class PreviewSettings;

class PreviewBridge : public KDecoration3::DecorationBridge
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(QString plugin READ plugin WRITE setPlugin NOTIFY pluginChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString kcmoduleName READ kcmoduleName WRITE setKcmoduleName NOTIFY kcmoduleNameChanged)
    Q_PROPERTY(bool valid READ isValid NOTIFY validChanged)
public:
    explicit PreviewBridge(QObject *parent = nullptr);
    ~PreviewBridge() override;
    std::unique_ptr<DecoratedWindowPrivate> createClient(DecoratedWindow *client, Decoration *decoration) override;
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
    void setKcmoduleName(const QString &name);
    QString kcmoduleName() const;
    void setTheme(const QString &theme);
    QString theme() const;
    bool isValid() const;

    KDecoration3::Decoration *createDecoration(QObject *parent = nullptr);
    KDecoration3::DecorationButton *createButton(KDecoration3::Decoration *decoration, KDecoration3::DecorationButtonType type, QObject *parent = nullptr);

public Q_SLOTS:
    void configure(QQuickItem *ctx);

Q_SIGNALS:
    void pluginChanged();
    void themeChanged();
    void validChanged();
    void kcmoduleNameChanged();

private:
    void createFactory();
    void setValid(bool valid);
    PreviewClient *m_lastCreatedClient;
    PreviewSettings *m_lastCreatedSettings;
    QList<PreviewItem *> m_previewItems;
    QString m_plugin;
    QString m_theme;
    QString m_kcmoduleName;
    QPointer<KPluginFactory> m_factory;
    bool m_valid;
};

class BridgeItem : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Bridge)
    Q_PROPERTY(QString plugin READ plugin WRITE setPlugin NOTIFY pluginChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString kcmoduleName READ kcmoduleName WRITE setKcmoduleName NOTIFY kcmoduleNameChanged)
    Q_PROPERTY(bool valid READ isValid NOTIFY validChanged)
    Q_PROPERTY(KDecoration3::Preview::PreviewBridge *bridge READ bridge CONSTANT)

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
    QString kcmoduleName() const
    {
        return m_bridge->kcmoduleName();
    }
    void setKcmoduleName(const QString &name)
    {
        m_bridge->setKcmoduleName(name);
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
    void kcmoduleNameChanged();
    void validChanged();

private:
    PreviewBridge *m_bridge;
};

}
}

Q_DECLARE_METATYPE(KDecoration3::Preview::PreviewBridge *)
