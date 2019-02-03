/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>
Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>

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

#include "effectmodel.h"

#include <config-kwin.h>
#include <effect_builtins.h>
#include <kwin_effects_interface.h>

#include <KAboutData>
#include <KCModule>
#include <KLocalizedString>
#include <KPackage/PackageLoader>
#include <KPluginLoader>
#include <KPluginMetaData>
#include <KPluginTrader>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace KWin
{

static QString translatedCategory(const QString &category)
{
    static const QVector<QString> knownCategories = {
        QStringLiteral("Accessibility"),
        QStringLiteral("Appearance"),
        QStringLiteral("Candy"),
        QStringLiteral("Focus"),
        QStringLiteral("Show Desktop Animation"),
        QStringLiteral("Tools"),
        QStringLiteral("Virtual Desktop Switching Animation"),
        QStringLiteral("Window Management"),
        QStringLiteral("Window Open/Close Animation")
    };

    static const QVector<QString> translatedCategories = {
        i18nc("Category of Desktop Effects, used as section header", "Accessibility"),
        i18nc("Category of Desktop Effects, used as section header", "Appearance"),
        i18nc("Category of Desktop Effects, used as section header", "Candy"),
        i18nc("Category of Desktop Effects, used as section header", "Focus"),
        i18nc("Category of Desktop Effects, used as section header", "Show Desktop Animation"),
        i18nc("Category of Desktop Effects, used as section header", "Tools"),
        i18nc("Category of Desktop Effects, used as section header", "Virtual Desktop Switching Animation"),
        i18nc("Category of Desktop Effects, used as section header", "Window Management"),
        i18nc("Category of Desktop Effects, used as section header", "Window Open/Close Animation")
    };

    const int index = knownCategories.indexOf(category);
    if (index == -1) {
        qDebug() << "Unknown category '" << category << "' and thus not translated";
        return category;
    }

    return translatedCategories[index];
}

static EffectModel::Status effectStatus(bool enabled)
{
    return enabled ? EffectModel::Status::Enabled : EffectModel::Status::Disabled;
}

EffectModel::EffectModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

QHash<int, QByteArray> EffectModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[NameRole] = "NameRole";
    roleNames[DescriptionRole] = "DescriptionRole";
    roleNames[AuthorNameRole] = "AuthorNameRole";
    roleNames[AuthorEmailRole] = "AuthorEmailRole";
    roleNames[LicenseRole] = "LicenseRole";
    roleNames[VersionRole] = "VersionRole";
    roleNames[CategoryRole] = "CategoryRole";
    roleNames[ServiceNameRole] = "ServiceNameRole";
    roleNames[IconNameRole] = "IconNameRole";
    roleNames[StatusRole] = "StatusRole";
    roleNames[VideoRole] = "VideoRole";
    roleNames[WebsiteRole] = "WebsiteRole";
    roleNames[SupportedRole] = "SupportedRole";
    roleNames[ExclusiveRole] = "ExclusiveRole";
    roleNames[ConfigurableRole] = "ConfigurableRole";
    roleNames[ScriptedRole] = QByteArrayLiteral("ScriptedRole");
    roleNames[EnabledByDefaultRole] = "EnabledByDefaultRole";
    return roleNames;
}

QModelIndex EffectModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() || column > 0 || column < 0 || row < 0 || row >= m_effectsList.count()) {
        return {};
    }

    return createIndex(row, column);
}

QModelIndex EffectModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child)
    return {};
}

int EffectModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

int EffectModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_effectsList.count();
}

QVariant EffectModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    const EffectData effect = m_effectsList.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return effect.name;
    case DescriptionRole:
        return effect.description;
    case AuthorNameRole:
        return effect.authorName;
    case AuthorEmailRole:
        return effect.authorEmail;
    case LicenseRole:
        return effect.license;
    case VersionRole:
        return effect.version;
    case CategoryRole:
        return effect.category;
    case ServiceNameRole:
        return effect.serviceName;
    case IconNameRole:
        return effect.iconName;
    case StatusRole:
        return static_cast<int>(effect.status);
    case VideoRole:
        return effect.video;
    case WebsiteRole:
        return effect.website;
    case SupportedRole:
        return effect.supported;
    case ExclusiveRole:
        return effect.exclusiveGroup;
    case InternalRole:
        return effect.internal;
    case ConfigurableRole:
        return effect.configurable;
    case ScriptedRole:
        return effect.kind == Kind::Scripted;
    case EnabledByDefaultRole:
        return effect.enabledByDefault;
    default:
        return {};
    }
}

bool EffectModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid()) {
        return QAbstractItemModel::setData(index, value, role);
    }

    if (role == StatusRole) {
        // note: whenever the StatusRole is modified (even to the same value) the entry
        // gets marked as changed and will get saved to the config file. This means the
        // config file could get polluted
        EffectData &data = m_effectsList[index.row()];
        data.status = Status(value.toInt());
        data.changed = data.status != data.originalStatus;
        emit dataChanged(index, index);

        if (data.status == Status::Enabled && !data.exclusiveGroup.isEmpty()) {
            // need to disable all other exclusive effects in the same category
            for (int i = 0; i < m_effectsList.size(); ++i) {
                if (i == index.row()) {
                    continue;
                }
                EffectData &otherData = m_effectsList[i];
                if (otherData.exclusiveGroup == data.exclusiveGroup) {
                    otherData.status = Status::Disabled;
                    otherData.changed = otherData.status != otherData.originalStatus;
                    emit dataChanged(this->index(i, 0), this->index(i, 0));
                }
            }
        }

        return true;
    }

    return QAbstractItemModel::setData(index, value, role);
}

void EffectModel::loadBuiltInEffects(const KConfigGroup &kwinConfig, const KPluginInfo::List &configs)
{
    const auto builtins = BuiltInEffects::availableEffects();
    for (auto builtin : builtins) {
        const BuiltInEffects::EffectData &data = BuiltInEffects::effectData(builtin);
        EffectData effect;
        effect.name = data.displayName;
        effect.description = data.comment;
        effect.authorName = i18n("KWin development team");
        effect.authorEmail = QString(); // not used at all
        effect.license = QStringLiteral("GPL");
        effect.version = QStringLiteral(KWIN_VERSION_STRING);
        effect.untranslatedCategory = data.category;
        effect.category = translatedCategory(data.category);
        effect.serviceName = data.name;
        effect.iconName = QStringLiteral("preferences-system-windows");
        effect.enabledByDefault = data.enabled;
        effect.enabledByDefaultFunction = (data.enabledFunction != nullptr);
        const QString enabledKey = QStringLiteral("%1Enabled").arg(effect.serviceName);
        if (kwinConfig.hasKey(enabledKey)) {
            effect.status = effectStatus(kwinConfig.readEntry(effect.serviceName + "Enabled", effect.enabledByDefault));
        } else if (data.enabledFunction != nullptr) {
            effect.status = Status::EnabledUndeterminded;
        } else {
            effect.status = effectStatus(effect.enabledByDefault);
        }
        effect.originalStatus = effect.status;
        effect.video = data.video;
        effect.website = QUrl();
        effect.supported = true;
        effect.exclusiveGroup = data.exclusiveCategory;
        effect.internal = data.internal;
        effect.kind = Kind::BuiltIn;

        effect.configurable = std::any_of(configs.constBegin(), configs.constEnd(),
            [data](const KPluginInfo &info) {
                return info.property(QStringLiteral("X-KDE-ParentComponents")).toString() == data.name;
            }
        );

        if (shouldStore(effect)) {
            m_effectsList << effect;
        }
    }
}

void EffectModel::loadJavascriptEffects(const KConfigGroup &kwinConfig)
{
    const auto plugins = KPackage::PackageLoader::self()->listPackages(
        QStringLiteral("KWin/Effect"),
        QStringLiteral("kwin/effects")
    );
    for (const KPluginMetaData &metaData : plugins) {
        KPluginInfo plugin(metaData);
        EffectData effect;

        effect.name = plugin.name();
        effect.description = plugin.comment();
        effect.authorName = plugin.author();
        effect.authorEmail = plugin.email();
        effect.license = plugin.license();
        effect.version = plugin.version();
        effect.untranslatedCategory = plugin.category();
        effect.category = translatedCategory(plugin.category());
        effect.serviceName = plugin.pluginName();
        effect.iconName = plugin.icon();
        effect.status = effectStatus(kwinConfig.readEntry(effect.serviceName + "Enabled", plugin.isPluginEnabledByDefault()));
        effect.originalStatus = effect.status;
        effect.enabledByDefault = plugin.isPluginEnabledByDefault();
        effect.enabledByDefaultFunction = false;
        effect.video = plugin.property(QStringLiteral("X-KWin-Video-Url")).toUrl();
        effect.website = QUrl(plugin.website());
        effect.supported = true;
        effect.exclusiveGroup = plugin.property(QStringLiteral("X-KWin-Exclusive-Category")).toString();
        effect.internal = plugin.property(QStringLiteral("X-KWin-Internal")).toBool();
        effect.kind = Kind::Scripted;

        const QString pluginKeyword = plugin.property(QStringLiteral("X-KDE-PluginKeyword")).toString();
        if (!pluginKeyword.isEmpty()) {
             // scripted effects have their pluginName() as the keyword
             effect.configurable = plugin.property(QStringLiteral("X-KDE-ParentComponents")).toString() == pluginKeyword;
        } else {
            effect.configurable = false;
        }

        if (shouldStore(effect)) {
            m_effectsList << effect;
        }
    }
}

void EffectModel::loadPluginEffects(const KConfigGroup &kwinConfig, const KPluginInfo::List &configs)
{
    const auto pluginEffects = KPluginLoader::findPlugins(
        QStringLiteral("kwin/effects/plugins/"),
        [](const KPluginMetaData &data) {
            return data.serviceTypes().contains(QStringLiteral("KWin/Effect"));
        }
    );
    for (const KPluginMetaData &pluginEffect : pluginEffects) {
        if (!pluginEffect.isValid()) {
            continue;
        }
        EffectData effect;
        effect.name = pluginEffect.name();
        effect.description = pluginEffect.description();
        effect.license = pluginEffect.license();
        effect.version = pluginEffect.version();
        effect.untranslatedCategory = pluginEffect.category();
        effect.category = translatedCategory(pluginEffect.category());
        effect.serviceName = pluginEffect.pluginId();
        effect.iconName = pluginEffect.iconName();
        effect.enabledByDefault = pluginEffect.isEnabledByDefault();
        effect.supported = true;
        effect.enabledByDefaultFunction = false;
        effect.internal = false;
        effect.kind = Kind::Binary;

        for (int i = 0; i < pluginEffect.authors().count(); ++i) {
            effect.authorName.append(pluginEffect.authors().at(i).name());
            effect.authorEmail.append(pluginEffect.authors().at(i).emailAddress());
            if (i+1 < pluginEffect.authors().count()) {
                effect.authorName.append(", ");
                effect.authorEmail.append(", ");
            }
        }

        if (pluginEffect.rawData().contains("org.kde.kwin.effect")) {
            const QJsonObject d(pluginEffect.rawData().value("org.kde.kwin.effect").toObject());
            effect.exclusiveGroup = d.value("exclusiveGroup").toString();
            effect.video = QUrl::fromUserInput(d.value("video").toString());
            effect.enabledByDefaultFunction = d.value("enabledByDefaultMethod").toBool();
        }

        effect.website = QUrl(pluginEffect.website());

        const QString enabledKey = QStringLiteral("%1Enabled").arg(effect.serviceName);
        if (kwinConfig.hasKey(enabledKey)) {
            effect.status = effectStatus(kwinConfig.readEntry(effect.serviceName + "Enabled", effect.enabledByDefault));
        } else if (effect.enabledByDefaultFunction) {
            effect.status = Status::EnabledUndeterminded;
        } else {
            effect.status = effectStatus(effect.enabledByDefault);
        }

        effect.originalStatus = effect.status;

        effect.configurable = std::any_of(configs.constBegin(), configs.constEnd(),
            [pluginEffect](const KPluginInfo &info) {
                return info.property(QStringLiteral("X-KDE-ParentComponents")).toString() == pluginEffect.pluginId();
            }
        );

        if (shouldStore(effect)) {
            m_effectsList << effect;
        }
    }
}

void EffectModel::load(LoadOptions options)
{
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Plugins");

    const QVector<EffectData> oldEffects = m_effectsList;

    beginResetModel();
    m_effectsList.clear();
    const KPluginInfo::List configs = KPluginTrader::self()->query(QStringLiteral("kwin/effects/configs/"));
    loadBuiltInEffects(kwinConfig, configs);
    loadJavascriptEffects(kwinConfig);
    loadPluginEffects(kwinConfig, configs);

    if (options == LoadOptions::KeepDirty) {
        for (const EffectData &oldEffect : oldEffects) {
            if (!oldEffect.changed) {
                continue;
            }
            auto effectIt = std::find_if(m_effectsList.begin(), m_effectsList.end(),
                [serviceName = oldEffect.serviceName](const EffectData &data) {
                    return data.serviceName == serviceName;
                }
            );
            if (effectIt == m_effectsList.end()) {
                continue;
            }
            effectIt->status = oldEffect.status;
            effectIt->changed = effectIt->status != effectIt->originalStatus;
        }
    }

    qSort(m_effectsList.begin(), m_effectsList.end(), [](const EffectData &a, const EffectData &b) {
        if (a.category == b.category) {
            if (a.exclusiveGroup == b.exclusiveGroup) {
                return a.name < b.name;
            }
            return a.exclusiveGroup < b.exclusiveGroup;
        }
        return a.category < b.category;
    });

    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());

    if (interface.isValid()) {
        QStringList effectNames;
        effectNames.reserve(m_effectsList.count());
        for (const EffectData &data : m_effectsList) {
            effectNames.append(data.serviceName);
        }

        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(interface.areEffectsSupported(effectNames), this);
        watcher->setProperty("effectNames", effectNames);
        connect(watcher, &QDBusPendingCallWatcher::finished, [this](QDBusPendingCallWatcher *self) {
            const QStringList effectNames = self->property("effectNames").toStringList();
            const QDBusPendingReply<QList<bool > > reply = *self;
            QList<bool> supportValues;
            if (reply.isValid()) {
                supportValues.append(reply.value());
            }
            if (effectNames.size() == supportValues.size()) {
                for (int i = 0; i < effectNames.size(); ++i) {
                    const bool supportedValue = supportValues.at(i);
                    const QString &effectName = effectNames.at(i);
                    auto it = std::find_if(m_effectsList.begin(), m_effectsList.end(), [effectName](const EffectData &data) {
                        return data.serviceName == effectName;
                    });
                    if (it != m_effectsList.end()) {
                        if ((*it).supported != supportedValue) {
                            (*it).supported = supportedValue;
                            QModelIndex i = findByPluginId(effectName);
                            if (i.isValid()) {
                                emit dataChanged(i, i, QVector<int>() << SupportedRole);
                            }
                        }
                    }
                }
            }
            self->deleteLater();
        });
    }

    endResetModel();
}

void EffectModel::updateEffectStatus(const QModelIndex &rowIndex, Status effectState)
{
    setData(rowIndex, static_cast<int>(effectState), StatusRole);
}

void EffectModel::save()
{
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Plugins");

    QVector<EffectData> dirtyEffects;

    for (EffectData &effect : m_effectsList) {
        if (!effect.changed) {
            continue;
        }

        effect.changed = false;
        effect.originalStatus = effect.status;

        const QString key = effect.serviceName + QStringLiteral("Enabled");
        const bool shouldEnable = (effect.status != Status::Disabled);
        const bool restoreToDefault = effect.enabledByDefaultFunction
            ? effect.status == Status::EnabledUndeterminded
            : shouldEnable == effect.enabledByDefault;
        if (restoreToDefault) {
            kwinConfig.deleteEntry(key);
        } else {
            kwinConfig.writeEntry(key, shouldEnable);
        }

        dirtyEffects.append(effect);
    }

    if (dirtyEffects.isEmpty()) {
        return;
    }

    kwinConfig.sync();

    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());

    if (!interface.isValid()) {
        return;
    }

    for (const EffectData &effect : dirtyEffects) {
        if (effect.status != Status::Disabled) {
            interface.loadEffect(effect.serviceName);
        } else {
            interface.unloadEffect(effect.serviceName);
        }
    }
}

void EffectModel::defaults()
{
    for (int i = 0; i < m_effectsList.count(); ++i) {
        const auto &effect = m_effectsList.at(i);
        if (effect.enabledByDefaultFunction && effect.status != Status::EnabledUndeterminded) {
            updateEffectStatus(index(i, 0), Status::EnabledUndeterminded);
        } else if ((bool)effect.status != effect.enabledByDefault) {
            updateEffectStatus(index(i, 0), effect.enabledByDefault ? Status::Enabled : Status::Disabled);
        }
    }
}

bool EffectModel::needsSave() const
{
    return std::any_of(m_effectsList.constBegin(), m_effectsList.constEnd(),
        [](const EffectData &data) {
            return data.changed;
        }
    );
}

QModelIndex EffectModel::findByPluginId(const QString &pluginId) const
{
    auto it = std::find_if(m_effectsList.constBegin(), m_effectsList.constEnd(),
        [pluginId](const EffectData &data) {
            return data.serviceName == pluginId;
        }
    );
    if (it == m_effectsList.constEnd()) {
        return {};
    }
    return index(std::distance(m_effectsList.constBegin(), it), 0);
}

static KCModule *findBinaryConfig(const QString &pluginId, QObject *parent)
{
    return KPluginTrader::createInstanceFromQuery<KCModule>(
        QStringLiteral("kwin/effects/configs/"),
        QString(),
        QStringLiteral("'%1' in [X-KDE-ParentComponents]").arg(pluginId),
        parent
    );
}

static KCModule *findScriptedConfig(const QString &pluginId, QObject *parent)
{
    const auto offers = KPluginTrader::self()->query(
        QStringLiteral("kwin/effects/configs/"),
        QString(),
        QStringLiteral("[X-KDE-Library] == 'kcm_kwin4_genericscripted'")
    );

    if (offers.isEmpty()) {
        return nullptr;
    }

    const KPluginInfo &generic = offers.first();
    KPluginLoader loader(generic.libraryPath());
    KPluginFactory *factory = loader.factory();
    if (!factory) {
        return nullptr;
    }

    return factory->create<KCModule>(pluginId, parent);
}

void EffectModel::requestConfigure(const QModelIndex &index, QWindow *transientParent)
{
    if (!index.isValid()) {
        return;
    }

    QPointer<QDialog> dialog = new QDialog();

    KCModule *module = index.data(ScriptedRole).toBool()
        ? findScriptedConfig(index.data(ServiceNameRole).toString(), dialog)
        : findBinaryConfig(index.data(ServiceNameRole).toString(), dialog);
    if (!module) {
        delete dialog;
        return;
    }

    dialog->setWindowTitle(index.data(NameRole).toString());
    dialog->winId();
    dialog->windowHandle()->setTransientParent(transientParent);

    auto buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok |
        QDialogButtonBox::Cancel |
        QDialogButtonBox::RestoreDefaults,
        dialog
    );
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked,
        module, &KCModule::defaults);

    auto layout = new QVBoxLayout(dialog);
    layout->addWidget(module);
    layout->addWidget(buttons);

    dialog->exec();

    delete dialog;
}

bool EffectModel::shouldStore(const EffectData &data) const
{
    Q_UNUSED(data)
    return true;
}

}
