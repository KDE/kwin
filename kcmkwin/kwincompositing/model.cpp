/**************************************************************************
* KWin - the KDE window manager                                          *
* This file is part of the KDE project.                                  *
*                                                                        *
* Copyright (C) 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>             *
*                                                                        *
* This program is free software; you can redistribute it and/or modify   *
* it under the terms of the GNU General Public License as published by   *
* the Free Software Foundation; either version 2 of the License, or      *
* (at your option) any later version.                                    *
*                                                                        *
* This program is distributed in the hope that it will be useful,        *
* but WITHOUT ANY WARRANTY; without even the implied warranty of         *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
* GNU General Public License for more details.                           *
*                                                                        *
* You should have received a copy of the GNU General Public License      *
* along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
**************************************************************************/

#include "model.h"
#include "effectconfig.h"
#include "compositing.h"
#include <config-kwin.h>
#include <kwin_effects_interface.h>
#include <effect_builtins.h>

#include <KLocalizedString>
#include <KSharedConfig>
#include <KCModuleProxy>
#include <KPackage/PackageLoader>
#include <KPluginTrader>
#include <kdeclarative/kdeclarative.h>
#include <KPluginLoader>
#include <KPluginMetaData>
#include <KAboutData>

#include <QAbstractItemModel>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusMessage>
#include <QHash>
#include <QVariant>
#include <QList>
#include <QString>
#include <QQmlEngine>
#include <QtQml>
#include <QQuickItem>
#include <QDebug>

namespace KWin {
namespace Compositing {

static QString translatedCategory(const QString &category)
{
    static const QVector<QString> knownCategories  = {
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

static EffectStatus effectStatus(bool enabled)
{
    return enabled ? EffectStatus::Enabled : EffectStatus::Disabled;
}

EffectModel::EffectModel(QObject *parent)
    : QAbstractItemModel(parent) {
}

QHash< int, QByteArray > EffectModel::roleNames() const
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
    roleNames[EffectStatusRole] = "EffectStatusRole";
    roleNames[VideoRole] = "VideoRole";
    roleNames[SupportedRole] = "SupportedRole";
    roleNames[ExclusiveRole] = "ExclusiveRole";
    roleNames[ConfigurableRole] = "ConfigurableRole";
    roleNames[ScriptedRole] = QByteArrayLiteral("ScriptedRole");
    return roleNames;
}

QModelIndex EffectModel::index(int row, int column, const QModelIndex &parent) const
{
if (parent.isValid() || column > 0 || column < 0 || row < 0 || row >= m_effectsList.count()) {
        return QModelIndex();
    }

    return createIndex(row, column);
}

QModelIndex EffectModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child)

    return QModelIndex();
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
        return QVariant();
    }

    EffectData currentEffect = m_effectsList.at(index.row());
    switch (role) {
        case Qt::DisplayRole:
        case NameRole:
            return m_effectsList.at(index.row()).name;
        case DescriptionRole:
            return m_effectsList.at(index.row()).description;
        case AuthorNameRole:
            return m_effectsList.at(index.row()).authorName;
        case AuthorEmailRole:
            return m_effectsList.at(index.row()).authorEmail;
        case LicenseRole:
            return m_effectsList.at(index.row()).license;
        case VersionRole:
            return m_effectsList.at(index.row()).version;
        case CategoryRole:
            return m_effectsList.at(index.row()).category;
        case ServiceNameRole:
            return m_effectsList.at(index.row()).serviceName;
        case EffectStatusRole:
            return (int)m_effectsList.at(index.row()).effectStatus;
        case VideoRole:
            return m_effectsList.at(index.row()).video;
        case SupportedRole:
            return m_effectsList.at(index.row()).supported;
        case ExclusiveRole:
            return m_effectsList.at(index.row()).exclusiveGroup;
        case InternalRole:
            return m_effectsList.at(index.row()).internal;
        case ConfigurableRole:
            return m_effectsList.at(index.row()).configurable;
        case ScriptedRole:
            return m_effectsList.at(index.row()).scripted;
        default:
            return QVariant();
    }
}

bool EffectModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
        return QAbstractItemModel::setData(index, value, role);

    if (role == EffectModel::EffectStatusRole) {
        // note: whenever the StatusRole is modified (even to the same value) the entry
        // gets marked as changed and will get saved to the config file. This means the
        // config file could get polluted
        EffectData &data = m_effectsList[index.row()];
        data.effectStatus = EffectStatus(value.toInt());
        data.changed = true;
        emit dataChanged(index, index);

        if (data.effectStatus == EffectStatus::Enabled && !data.exclusiveGroup.isEmpty()) {
            // need to disable all other exclusive effects in the same category
            for (int i = 0; i < m_effectsList.size(); ++i) {
                if (i == index.row()) {
                    continue;
                }
                EffectData &otherData = m_effectsList[i];
                if (otherData.exclusiveGroup == data.exclusiveGroup) {
                    otherData.effectStatus = EffectStatus::Disabled;
                    otherData.changed = true;
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
        effect.category = translatedCategory(data.category);
        effect.serviceName = data.name;
        effect.enabledByDefault = data.enabled;
        effect.enabledByDefaultFunction = (data.enabledFunction != nullptr);
        const QString enabledKey = QStringLiteral("%1Enabled").arg(effect.serviceName);
        if (kwinConfig.hasKey(enabledKey)) {
            effect.effectStatus = effectStatus(kwinConfig.readEntry(effect.serviceName + "Enabled", effect.enabledByDefault));
        } else if (data.enabledFunction != nullptr) {
            effect.effectStatus = EffectStatus::EnabledUndeterminded;
        } else {
            effect.effectStatus = effectStatus(effect.enabledByDefault);
        }
        effect.video = data.video;
        effect.supported = true;
        effect.exclusiveGroup = data.exclusiveCategory;
        effect.internal = data.internal;
        effect.scripted = false;

        auto it = std::find_if(configs.begin(), configs.end(), [data](const KPluginInfo &info) {
            return info.property(QStringLiteral("X-KDE-ParentComponents")).toString() == data.name;
        });
        effect.configurable = it != configs.end();

        m_effectsList << effect;
    }
}

void EffectModel::loadJavascriptEffects(const KConfigGroup &kwinConfig)
{
    const auto plugins = KPackage::PackageLoader::self()->listPackages(QStringLiteral("KWin/Effect"), QStringLiteral("kwin/effects"));
    for (const KPluginMetaData &metaData : plugins) {
        KPluginInfo plugin(metaData);
        EffectData effect;

        effect.name = plugin.name();
        effect.description = plugin.comment();
        effect.authorName = plugin.author();
        effect.authorEmail = plugin.email();
        effect.license = plugin.license();
        effect.version = plugin.version();
        effect.category = translatedCategory(plugin.category());
        effect.serviceName = plugin.pluginName();
        effect.effectStatus = effectStatus(kwinConfig.readEntry(effect.serviceName + "Enabled", plugin.isPluginEnabledByDefault()));
        effect.enabledByDefault = plugin.isPluginEnabledByDefault();
        effect.enabledByDefaultFunction = false;
        effect.video = plugin.property(QStringLiteral("X-KWin-Video-Url")).toUrl();
        effect.supported = true;
        effect.exclusiveGroup = plugin.property(QStringLiteral("X-KWin-Exclusive-Category")).toString();
        effect.internal = plugin.property(QStringLiteral("X-KWin-Internal")).toBool();
        effect.scripted = true;

        const QString pluginKeyword = plugin.property(QStringLiteral("X-KDE-PluginKeyword")).toString();
        if (!pluginKeyword.isEmpty()) {
             // scripted effects have their pluginName() as the keyword
             effect.configurable = plugin.property(QStringLiteral("X-KDE-ParentComponents")).toString() == pluginKeyword;
        } else {
            effect.configurable = false;
        }

        m_effectsList << effect;
    }
}

void EffectModel::loadPluginEffects(const KConfigGroup &kwinConfig, const KPluginInfo::List &configs)
{
    static const QString subDir(QStringLiteral("kwin/effects/plugins/"));
    static const QString serviceType(QStringLiteral("KWin/Effect"));
    const QVector<KPluginMetaData> pluginEffects = KPluginLoader::findPlugins(subDir, [] (const KPluginMetaData &data) { return data.serviceTypes().contains(serviceType); });
    for (KPluginMetaData pluginEffect : pluginEffects) {
        if (!pluginEffect.isValid())
            continue;
        EffectData effect;
        effect.name = pluginEffect.name();
        effect.description = pluginEffect.description();
        effect.license = pluginEffect.license();
        effect.version = pluginEffect.version();
        effect.category = pluginEffect.category();
        effect.serviceName = pluginEffect.pluginId();
        effect.enabledByDefault = pluginEffect.isEnabledByDefault();
        effect.supported = true;
        effect.enabledByDefaultFunction = false;
        effect.internal = false;
        effect.scripted = false;

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

        const QString enabledKey = QStringLiteral("%1Enabled").arg(effect.serviceName);
        if (kwinConfig.hasKey(enabledKey)) {
            effect.effectStatus = effectStatus(kwinConfig.readEntry(effect.serviceName + "Enabled", effect.enabledByDefault));
        } else if (effect.enabledByDefaultFunction) {
            effect.effectStatus = EffectStatus::EnabledUndeterminded;
        } else {
            effect.effectStatus = effectStatus(effect.enabledByDefault);
        }

        auto it = std::find_if(configs.begin(), configs.end(), [pluginEffect](const KPluginInfo &info) {
            return info.property(QStringLiteral("X-KDE-ParentComponents")).toString() == pluginEffect.pluginId();
        });
        effect.configurable = it != configs.end();

        m_effectsList << effect;
    }
}

void EffectModel::loadEffects()
{
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Plugins");

    beginResetModel();
    m_effectsChanged.clear();
    m_effectsList.clear();
    const KPluginInfo::List configs = KPluginTrader::self()->query(QStringLiteral("kwin/effects/configs/"));
    loadBuiltInEffects(kwinConfig, configs);
    loadJavascriptEffects(kwinConfig);
    loadPluginEffects(kwinConfig, configs);

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
        std::for_each(m_effectsList.constBegin(), m_effectsList.constEnd(), [&effectNames](const EffectData &data) {
            effectNames << data.serviceName;
        });
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(interface.areEffectsSupported(effectNames), this);
        watcher->setProperty("effectNames", effectNames);
        connect(watcher, &QDBusPendingCallWatcher::finished, [this](QDBusPendingCallWatcher *self) {
            const QStringList effectNames = self->property("effectNames").toStringList();
            const QDBusPendingReply< QList< bool > > reply = *self;
            QList< bool> supportValues;
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
                            QModelIndex i = index(findRowByServiceName(effectName), 0);
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

    m_effectsChanged = m_effectsList;
    endResetModel();
}

int EffectModel::findRowByServiceName(const QString &serviceName)
{
    for (int it = 0; it < m_effectsList.size(); it++) {
        if (m_effectsList.at(it).serviceName == serviceName) {
            return it;
        }
    }
    return -1;
}

void EffectModel::syncEffectsToKWin()
{
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                             QStringLiteral("/Effects"),
                                             QDBusConnection::sessionBus());
    for (int it = 0; it < m_effectsList.size(); it++) {
        if (m_effectsList.at(it).effectStatus != m_effectsChanged.at(it).effectStatus) {
            if (m_effectsList.at(it).effectStatus != EffectStatus::Disabled) {
                interface.loadEffect(m_effectsList.at(it).serviceName);
            } else {
                interface.unloadEffect(m_effectsList.at(it).serviceName);
            }
        }
    }

    m_effectsChanged = m_effectsList;
}

void EffectModel::updateEffectStatus(const QModelIndex &rowIndex, EffectStatus effectState)
{
    setData(rowIndex, (int)effectState, EffectModel::EffectStatusRole);
}

void EffectModel::syncConfig()
{
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Plugins");

    for (auto it = m_effectsList.begin(); it != m_effectsList.end(); it++) {
        EffectData &effect = *(it);
        if (!effect.changed) {
            continue;
        }
        effect.changed = false;

        const QString key = effect.serviceName + QStringLiteral("Enabled");
        const bool shouldEnable = (effect.effectStatus != EffectStatus::Disabled);
        const bool restoreToDefault = effect.enabledByDefaultFunction
                                      ? effect.effectStatus == EffectStatus::EnabledUndeterminded
                                      : shouldEnable == effect.enabledByDefault;
        if (restoreToDefault) {
            kwinConfig.deleteEntry(key);
        } else {
            kwinConfig.writeEntry(key, shouldEnable);
        }
    }

    kwinConfig.sync();
    syncEffectsToKWin();
}

void EffectModel::defaults()
{
    for (int i = 0; i < m_effectsList.count(); ++i) {
        const auto &effect = m_effectsList.at(i);
        if (effect.enabledByDefaultFunction && effect.effectStatus != EffectStatus::EnabledUndeterminded) {
            updateEffectStatus(index(i, 0), EffectStatus::EnabledUndeterminded);
        } else if ((bool)effect.effectStatus != effect.enabledByDefault) {
            updateEffectStatus(index(i, 0), effect.enabledByDefault ? EffectStatus::Enabled : EffectStatus::Disabled);
        }
    }
}

EffectFilterModel::EffectFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_effectModel(new EffectModel(this))
    , m_filterOutUnsupported(true)
    , m_filterOutInternal(true)
{
    setSourceModel(m_effectModel);
    connect(this, &EffectFilterModel::filterOutUnsupportedChanged, this, &EffectFilterModel::invalidateFilter);
    connect(this, &EffectFilterModel::filterOutInternalChanged, this, &EffectFilterModel::invalidateFilter);
}

const QString &EffectFilterModel::filter() const
{
    return m_filter;
}

void EffectFilterModel::setFilter(const QString &filter)
{
    if (filter == m_filter) {
        return;
    }

    m_filter = filter;
    emit filterChanged();
    invalidateFilter();
}

bool EffectFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (!m_effectModel) {
        return false;
    }

    QModelIndex index = m_effectModel->index(source_row, 0, source_parent);
    if (!index.isValid()) {
        return false;
    }

    if (m_filterOutUnsupported) {
        if (!index.data(EffectModel::SupportedRole).toBool()) {
            return false;
        }
    }

    if (m_filterOutInternal) {
        if (index.data(EffectModel::InternalRole).toBool()) {
            return false;
        }
    }

    if (m_filter.isEmpty()) {
        return true;
    }

    QVariant data = index.data();
    if (!data.isValid()) {
        //An invalid QVariant is valid data
        return true;
    }

    if (m_effectModel->data(index, EffectModel::NameRole).toString().contains(m_filter, Qt::CaseInsensitive)) {
        return true;
    } else if (m_effectModel->data(index, EffectModel::DescriptionRole).toString().contains(m_filter, Qt::CaseInsensitive)) {
        return true;
    }
    if (index.data(EffectModel::CategoryRole).toString().contains(m_filter, Qt::CaseInsensitive)) {
        return true;
    }

    return false;
}

void EffectFilterModel::updateEffectStatus(int rowIndex, int effectState)
{
    const QModelIndex sourceIndex = mapToSource(index(rowIndex, 0));

    m_effectModel->updateEffectStatus(sourceIndex, EffectStatus(effectState));
}

void EffectFilterModel::syncConfig()
{
    m_effectModel->syncConfig();
}

void EffectFilterModel::load()
{
    m_effectModel->loadEffects();
}

void EffectFilterModel::defaults()
{
    m_effectModel->defaults();
}

EffectView::EffectView(ViewType type, QWidget *parent)
    : QQuickWidget(parent)
{
    qRegisterMetaType<OpenGLPlatformInterfaceModel*>();
    qmlRegisterType<EffectConfig>("org.kde.kwin.kwincompositing", 1, 0, "EffectConfig");
    qmlRegisterType<EffectFilterModel>("org.kde.kwin.kwincompositing", 1, 0, "EffectFilterModel");
    qmlRegisterType<Compositing>("org.kde.kwin.kwincompositing", 1, 0, "Compositing");
    qmlRegisterType<CompositingType>("org.kde.kwin.kwincompositing", 1, 0, "CompositingType");
    init(type);
}

void EffectView::init(ViewType type)
{
    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
    kdeclarative.setTranslationDomain(QStringLiteral(TRANSLATION_DOMAIN));
    kdeclarative.setupContext();
    kdeclarative.setupEngine(engine());
    QString path;
    switch (type) {
    case CompositingSettingsView:
        path = QStringLiteral("kwincompositing/qml/main-compositing.qml");
        break;
    case DesktopEffectsView:
        path = QStringLiteral("kwincompositing/qml/main.qml");
        break;
    }
    QString mainFile = QStandardPaths::locate(QStandardPaths::GenericDataLocation, path, QStandardPaths::LocateFile);
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setSource(QUrl(mainFile));
    rootObject()->setProperty("color",
                              KColorScheme(QPalette::Active, KColorScheme::Window, KSharedConfigPtr(0)).background(KColorScheme::NormalBackground).color());
    connect(rootObject(), SIGNAL(changed()), this, SIGNAL(changed()));
    setMinimumSize(initialSize());
    connect(rootObject(), SIGNAL(implicitWidthChanged()), this, SLOT(slotImplicitSizeChanged()));
    connect(rootObject(), SIGNAL(implicitHeightChanged()), this, SLOT(slotImplicitSizeChanged()));
}

void EffectView::save()
{
    if (auto *model = rootObject()->findChild<EffectFilterModel*>(QStringLiteral("filterModel"))) {
        model->syncConfig();
    }
    if (auto *compositing = rootObject()->findChild<Compositing*>(QStringLiteral("compositing"))) {
        compositing->save();
    }
}

void EffectView::load()
{
    if (auto *model = rootObject()->findChild<EffectFilterModel*>(QStringLiteral("filterModel"))) {
        model->load();
    }
    if (auto *compositing = rootObject()->findChild<Compositing*>(QStringLiteral("compositing"))) {
        compositing->reset();
    }
}

void EffectView::defaults()
{
    if (auto *model = rootObject()->findChild<EffectFilterModel*>(QStringLiteral("filterModel"))) {
        model->defaults();
    }
    if (auto *compositing = rootObject()->findChild<Compositing*>(QStringLiteral("compositing"))) {
        compositing->defaults();
    }
}

void EffectView::slotImplicitSizeChanged()
{
    setMinimumSize(QSize(rootObject()->property("implicitWidth").toInt(),
                         rootObject()->property("implicitHeight").toInt()));
}

}//end namespace Compositing
}//end namespace KWin
