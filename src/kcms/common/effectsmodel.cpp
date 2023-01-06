/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "effectsmodel.h"

#include <config-kwin.h>

#include <kwin_effects_interface.h>

#include <KAboutData>
#include <KCModule>
#include <KCModuleLoader>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KPackage/PackageLoader>
#include <KPluginFactory>
#include <KPluginMetaData>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace KWin
{

static QString translatedCategory(const QString &category)
{
    static const QVector<QString> knownCategories = {
        QStringLiteral("Accessibility"),
        QStringLiteral("Appearance"),
        QStringLiteral("Focus"),
        QStringLiteral("Show Desktop Animation"),
        QStringLiteral("Tools"),
        QStringLiteral("Virtual Desktop Switching Animation"),
        QStringLiteral("Window Management"),
        QStringLiteral("Window Open/Close Animation")};

    static const QVector<QString> translatedCategories = {
        i18nc("Category of Desktop Effects, used as section header", "Accessibility"),
        i18nc("Category of Desktop Effects, used as section header", "Appearance"),
        i18nc("Category of Desktop Effects, used as section header", "Focus"),
        i18nc("Category of Desktop Effects, used as section header", "Peek at Desktop Animation"),
        i18nc("Category of Desktop Effects, used as section header", "Tools"),
        i18nc("Category of Desktop Effects, used as section header", "Virtual Desktop Switching Animation"),
        i18nc("Category of Desktop Effects, used as section header", "Window Management"),
        i18nc("Category of Desktop Effects, used as section header", "Window Open/Close Animation")};

    const int index = knownCategories.indexOf(category);
    if (index == -1) {
        qDebug() << "Unknown category '" << category << "' and thus not translated";
        return category;
    }

    return translatedCategories[index];
}

static EffectsModel::Status effectStatus(bool enabled)
{
    return enabled ? EffectsModel::Status::Enabled : EffectsModel::Status::Disabled;
}

EffectsModel::EffectsModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

QHash<int, QByteArray> EffectsModel::roleNames() const
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
    roleNames[EnabledByDefaultFunctionRole] = "EnabledByDefaultFunctionRole";
    roleNames[ConfigModuleRole] = "ConfigModuleRole";
    return roleNames;
}

QModelIndex EffectsModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() || column > 0 || column < 0 || row < 0 || row >= m_effects.count()) {
        return {};
    }

    return createIndex(row, column);
}

QModelIndex EffectsModel::parent(const QModelIndex &child) const
{
    return {};
}

int EffectsModel::columnCount(const QModelIndex &parent) const
{
    return 1;
}

int EffectsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_effects.count();
}

QVariant EffectsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    const EffectData effect = m_effects.at(index.row());
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
    case EnabledByDefaultFunctionRole:
        return effect.enabledByDefaultFunction;
    case ConfigModuleRole:
        return effect.configModule;
    default:
        return {};
    }
}

bool EffectsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid()) {
        return QAbstractItemModel::setData(index, value, role);
    }

    if (role == StatusRole) {
        // note: whenever the StatusRole is modified (even to the same value) the entry
        // gets marked as changed and will get saved to the config file. This means the
        // config file could get polluted
        EffectData &data = m_effects[index.row()];
        data.status = Status(value.toInt());
        data.changed = data.status != data.originalStatus;
        Q_EMIT dataChanged(index, index);

        if (data.status == Status::Enabled && !data.exclusiveGroup.isEmpty()) {
            // need to disable all other exclusive effects in the same category
            for (int i = 0; i < m_effects.size(); ++i) {
                if (i == index.row()) {
                    continue;
                }
                EffectData &otherData = m_effects[i];
                if (otherData.exclusiveGroup == data.exclusiveGroup) {
                    otherData.status = Status::Disabled;
                    otherData.changed = otherData.status != otherData.originalStatus;
                    Q_EMIT dataChanged(this->index(i, 0), this->index(i, 0));
                }
            }
        }

        return true;
    }

    return QAbstractItemModel::setData(index, value, role);
}

void EffectsModel::loadBuiltInEffects(const KConfigGroup &kwinConfig)
{
    const QString rootDirectory = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                         QStringLiteral("kwin/builtin-effects"),
                                                         QStandardPaths::LocateDirectory);

    const QStringList nameFilters{QStringLiteral("metadata.json")};
    QDirIterator it(rootDirectory, nameFilters, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();

        const KPluginMetaData metaData = KPluginMetaData::fromJsonFile(it.filePath());
        if (!metaData.isValid()) {
            continue;
        }

        EffectData effect;
        effect.name = metaData.name();
        effect.description = metaData.description();
        effect.authorName = i18n("KWin development team");
        effect.authorEmail = QString(); // not used at all
        effect.license = metaData.license();
        effect.version = metaData.version();
        effect.untranslatedCategory = metaData.category();
        effect.category = translatedCategory(metaData.category());
        effect.serviceName = metaData.pluginId();
        effect.iconName = metaData.iconName();
        effect.enabledByDefault = metaData.isEnabledByDefault();
        effect.supported = true;
        effect.enabledByDefaultFunction = false;
        effect.internal = false;
        effect.kind = Kind::BuiltIn;
        effect.configModule = metaData.value(QStringLiteral("X-KDE-ConfigModule"));
        effect.website = QUrl(metaData.website());

        if (metaData.rawData().contains("org.kde.kwin.effect")) {
            const QJsonObject d(metaData.rawData().value("org.kde.kwin.effect").toObject());
            effect.exclusiveGroup = d.value("exclusiveGroup").toString();
            effect.video = QUrl::fromUserInput(d.value("video").toString());
            effect.enabledByDefaultFunction = d.value("enabledByDefaultMethod").toBool();
            effect.internal = d.value("internal").toBool();
        }

        const QString enabledKey = QStringLiteral("%1Enabled").arg(effect.serviceName);
        if (kwinConfig.hasKey(enabledKey)) {
            effect.status = effectStatus(kwinConfig.readEntry(effect.serviceName + "Enabled", effect.enabledByDefault));
        } else if (effect.enabledByDefaultFunction) {
            effect.status = Status::EnabledUndeterminded;
        } else {
            effect.status = effectStatus(effect.enabledByDefault);
        }

        effect.originalStatus = effect.status;
        effect.configurable = !effect.configModule.isEmpty();

        if (shouldStore(effect)) {
            m_pendingEffects << effect;
        }
    }
}

void EffectsModel::loadJavascriptEffects(const KConfigGroup &kwinConfig)
{
    const auto plugins = KPackage::PackageLoader::self()->listPackages(
        QStringLiteral("KWin/Effect"),
        QStringLiteral("kwin/effects"));
    for (const KPluginMetaData &plugin : plugins) {
        EffectData effect;

        effect.name = plugin.name();
        effect.description = plugin.description();
        const auto authors = plugin.authors();
        effect.authorName = !authors.isEmpty() ? authors.first().name() : QString();
        effect.authorEmail = !authors.isEmpty() ? authors.first().emailAddress() : QString();
        effect.license = plugin.license();
        effect.version = plugin.version();
        effect.untranslatedCategory = plugin.category();
        effect.category = translatedCategory(plugin.category());
        effect.serviceName = plugin.pluginId();
        effect.iconName = plugin.iconName();
        effect.status = effectStatus(kwinConfig.readEntry(effect.serviceName + "Enabled", plugin.isEnabledByDefault()));
        effect.originalStatus = effect.status;
        effect.enabledByDefault = plugin.isEnabledByDefault();
        effect.enabledByDefaultFunction = false;
        effect.video = QUrl(plugin.value(QStringLiteral("X-KWin-Video-Url")));
        effect.website = QUrl(plugin.website());
        effect.supported = true;
        effect.exclusiveGroup = plugin.value(QStringLiteral("X-KWin-Exclusive-Category"));
        effect.internal = plugin.value(QStringLiteral("X-KWin-Internal"), false);
        effect.kind = Kind::Scripted;

        const QString pluginKeyword = plugin.value(QStringLiteral("X-KDE-PluginKeyword"));
        if (!pluginKeyword.isEmpty()) {
            QDir package(QFileInfo(plugin.metaDataFileName()).dir());
            package.cd(QStringLiteral("contents"));
            const QString xmlFile = package.filePath(QStringLiteral("config/main.xml"));
            const QString uiFile = package.filePath(QStringLiteral("ui/config.ui"));
            effect.configurable = QFileInfo::exists(xmlFile) && QFileInfo::exists(uiFile);
        } else {
            effect.configurable = false;
        }

        if (shouldStore(effect)) {
            m_pendingEffects << effect;
        }
    }
}

void EffectsModel::loadPluginEffects(const KConfigGroup &kwinConfig)
{
    const auto pluginEffects = KPluginMetaData::findPlugins(QStringLiteral("kwin/effects/plugins"));
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
        effect.configModule = pluginEffect.value(QStringLiteral("X-KDE-ConfigModule"));

        for (int i = 0; i < pluginEffect.authors().count(); ++i) {
            effect.authorName.append(pluginEffect.authors().at(i).name());
            effect.authorEmail.append(pluginEffect.authors().at(i).emailAddress());
            if (i + 1 < pluginEffect.authors().count()) {
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

        effect.configurable = !effect.configModule.isEmpty();

        if (shouldStore(effect)) {
            m_pendingEffects << effect;
        }
    }
}

void EffectsModel::load(LoadOptions options)
{
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Plugins");

    m_pendingEffects.clear();
    loadBuiltInEffects(kwinConfig);
    loadJavascriptEffects(kwinConfig);
    loadPluginEffects(kwinConfig);

    std::sort(m_pendingEffects.begin(), m_pendingEffects.end(),
              [](const EffectData &a, const EffectData &b) {
                  if (a.category == b.category) {
                      if (a.exclusiveGroup == b.exclusiveGroup) {
                          return a.name < b.name;
                      }
                      return a.exclusiveGroup < b.exclusiveGroup;
                  }
                  return a.category < b.category;
              });

    auto commit = [this, options] {
        if (options == LoadOptions::KeepDirty) {
            for (const EffectData &oldEffect : std::as_const(m_effects)) {
                if (!oldEffect.changed) {
                    continue;
                }
                auto effectIt = std::find_if(m_pendingEffects.begin(), m_pendingEffects.end(),
                                             [oldEffect](const EffectData &data) {
                                                 return data.serviceName == oldEffect.serviceName;
                                             });
                if (effectIt == m_pendingEffects.end()) {
                    continue;
                }
                effectIt->status = oldEffect.status;
                effectIt->changed = effectIt->status != effectIt->originalStatus;
            }
        }

        beginResetModel();
        m_effects = m_pendingEffects;
        m_pendingEffects.clear();
        endResetModel();

        Q_EMIT loaded();
    };

    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());

    if (interface.isValid()) {
        QStringList effectNames;
        effectNames.reserve(m_pendingEffects.count());
        for (const EffectData &data : std::as_const(m_pendingEffects)) {
            effectNames.append(data.serviceName);
        }

        const int serial = ++m_lastSerial;

        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(interface.areEffectsSupported(effectNames), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [=, this](QDBusPendingCallWatcher *self) {
            self->deleteLater();

            if (m_lastSerial != serial) {
                return;
            }

            const QDBusPendingReply<QList<bool>> reply = *self;
            if (reply.isError()) {
                commit();
                return;
            }

            const QList<bool> supportedValues = reply.value();
            if (supportedValues.count() != effectNames.count()) {
                return;
            }

            for (int i = 0; i < effectNames.size(); ++i) {
                const bool supported = supportedValues.at(i);
                const QString effectName = effectNames.at(i);

                auto it = std::find_if(m_pendingEffects.begin(), m_pendingEffects.end(),
                                       [effectName](const EffectData &data) {
                                           return data.serviceName == effectName;
                                       });
                if (it == m_pendingEffects.end()) {
                    continue;
                }

                if ((*it).supported != supported) {
                    (*it).supported = supported;
                }
            }

            commit();
        });
    } else {
        commit();
    }
}

void EffectsModel::updateEffectStatus(const QModelIndex &rowIndex, Status effectState)
{
    setData(rowIndex, static_cast<int>(effectState), StatusRole);
}

void EffectsModel::save()
{
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Plugins");

    QVector<EffectData> dirtyEffects;

    for (EffectData &effect : m_effects) {
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

    // Unload effects first, it's need to ensure that switching between mutually exclusive
    // effects works as expected, for example so global shortcuts are handed over, etc.
    auto split = std::partition(dirtyEffects.begin(), dirtyEffects.end(), [](const EffectData &data) {
        return data.status == Status::Disabled;
    });

    for (auto it = dirtyEffects.begin(); it != split; ++it) {
        interface.unloadEffect(it->serviceName);
    }

    for (auto it = split; it != dirtyEffects.end(); ++it) {
        interface.loadEffect(it->serviceName);
    }
}

void EffectsModel::defaults()
{
    for (int i = 0; i < m_effects.count(); ++i) {
        const auto &effect = m_effects.at(i);
        if (effect.enabledByDefaultFunction && effect.status != Status::EnabledUndeterminded) {
            updateEffectStatus(index(i, 0), Status::EnabledUndeterminded);
        } else if (static_cast<bool>(effect.status) != effect.enabledByDefault) {
            updateEffectStatus(index(i, 0), effect.enabledByDefault ? Status::Enabled : Status::Disabled);
        }
    }
}

bool EffectsModel::isDefaults() const
{
    return std::all_of(m_effects.constBegin(), m_effects.constEnd(), [](const EffectData &effect) {
        if (effect.enabledByDefaultFunction && effect.status != Status::EnabledUndeterminded) {
            return false;
        }
        if (static_cast<bool>(effect.status) != effect.enabledByDefault) {
            return false;
        }
        return true;
    });
}

bool EffectsModel::needsSave() const
{
    return std::any_of(m_effects.constBegin(), m_effects.constEnd(),
                       [](const EffectData &data) {
                           return data.changed;
                       });
}

QModelIndex EffectsModel::findByPluginId(const QString &pluginId) const
{
    auto it = std::find_if(m_effects.constBegin(), m_effects.constEnd(),
                           [pluginId](const EffectData &data) {
                               return data.serviceName == pluginId;
                           });
    if (it == m_effects.constEnd()) {
        return {};
    }
    return index(std::distance(m_effects.constBegin(), it), 0);
}

static KCModule *loadBinaryConfig(const QString &configModule, QWidget *parent)
{
    const KPluginMetaData metaData(QStringLiteral("kwin/effects/configs/") + configModule);
    return KCModuleLoader::loadModule(metaData, parent);
}

static KCModule *findScriptedConfig(const QString &pluginId, QObject *parent)
{
    KPluginMetaData metaData(QStringLiteral("kwin/effects/configs/kcm_kwin4_genericscripted"));
    return KPluginFactory::instantiatePlugin<KCModule>(metaData, parent, QVariantList{pluginId}).plugin;
}

void EffectsModel::requestConfigure(const QModelIndex &index, QWindow *transientParent)
{
    if (!index.isValid()) {
        return;
    }

    auto dialog = new QDialog();

    const bool scripted = index.data(ScriptedRole).toBool();

    KCModule *module = nullptr;

    if (scripted) {
        module = findScriptedConfig(index.data(ServiceNameRole).toString(), dialog);
    } else {
        const QString configModule = index.data(ConfigModuleRole).toString();
        module = loadBinaryConfig(configModule, dialog);
    }

    dialog->setWindowTitle(index.data(NameRole).toString());
    dialog->winId();
    dialog->windowHandle()->setTransientParent(transientParent);

    auto buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults,
        dialog);
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked,
            module, &KCModule::defaults);
    connect(module, &KCModule::defaulted, this, [=](bool defaulted) {
        buttons->button(QDialogButtonBox::RestoreDefaults)->setEnabled(!defaulted);
    });

    auto layout = new QVBoxLayout(dialog);
    layout->addWidget(module);
    layout->addWidget(buttons);

    connect(dialog, &QDialog::accepted, module, &KCModule::save);

    dialog->setModal(true);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

bool EffectsModel::shouldStore(const EffectData &data) const
{
    return true;
}

}
