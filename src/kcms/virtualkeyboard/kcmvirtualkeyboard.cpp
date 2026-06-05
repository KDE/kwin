/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>
    SPDX-FileCopyrightText: 2026 Kristen McWilliam <kristen@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kcmvirtualkeyboard.h"

#include <qqml.h>

#include <KApplicationTrader>
#include <KConfigGroup>
#include <KDesktopFile>
#include <KLocalizedString>
#include <KPluginFactory>

#include <virtualkeyboarddata.h>
#include <virtualkeyboardsettings.h>

K_PLUGIN_FACTORY_WITH_JSON(KcmVirtualKeyboardFactory, "kcm_virtualkeyboard.json", registerPlugin<KcmVirtualKeyboard>(); registerPlugin<VirtualKeyboardData>();)

KcmVirtualKeyboard::KcmVirtualKeyboard(QObject *parent, const KPluginMetaData &metaData)
    : KQuickManagedConfigModule(parent, metaData)
    , m_data(new VirtualKeyboardData(this))
    , m_model(new VirtualKeyboardsModel(this))
    , m_dbusInterface(std::make_unique<KWinVirtualKeyboard>())
    , m_mode(static_cast<KWinVirtualKeyboard::VirtualKeyboardMode>(m_dbusInterface->mode()))
{
    qmlRegisterAnonymousType<VirtualKeyboardSettings>("org.kde.kwin.virtualkeyboardsettings", 1);
    qmlRegisterType<KWinVirtualKeyboard>("org.kde.kwin.virtualkeyboard", 1, 0, "KWinVirtualKeyboard");
}

KcmVirtualKeyboard::~KcmVirtualKeyboard() = default;

VirtualKeyboardSettings *KcmVirtualKeyboard::settings() const
{
    return m_data->settings();
}

void KcmVirtualKeyboard::setMode(KWinVirtualKeyboard::VirtualKeyboardMode mode)
{
    if (m_mode == mode) {
        return;
    }
    m_mode = mode;
    Q_EMIT modeChanged(mode);
    setNeedsSave(true);
}

void KcmVirtualKeyboard::save()
{
    KQuickManagedConfigModule::save();
    m_dbusInterface->setMode(static_cast<int>(m_mode));
}

VirtualKeyboardsModel::VirtualKeyboardsModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_services = KApplicationTrader::query([](const KService::Ptr &service) {
        return service->property<bool>("X-KDE-Wayland-VirtualKeyboard");
    });

    m_services.prepend({});
}

QHash<int, QByteArray> VirtualKeyboardsModel::roleNames() const
{
    QHash<int, QByteArray> ret = QAbstractListModel::roleNames();
    ret.insert(DesktopFileNameRole, "desktopFileName");
    return ret;
}

QVariant VirtualKeyboardsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.parent().isValid() || index.row() > m_services.count()) {
        return {};
    }

    const KService::Ptr service = m_services[index.row()];
    switch (role) {
    case Qt::DisplayRole:
        return service ? service->name() : i18n("None");
    case Qt::DecorationRole:
        return service ? service->icon() : QStringLiteral("edit-none");
    case Qt::ToolTipRole:
        return service ? service->comment() : i18n("Do not use any virtual keyboard");
    case DesktopFileNameRole:
        return service ? QStandardPaths::locate(QStandardPaths::ApplicationsLocation, service->desktopEntryName() + QLatin1StringView(".desktop")) : QString();
    }
    return {};
}

int VirtualKeyboardsModel::inputMethodIndex(const QString &desktopFile) const
{
    if (desktopFile.isEmpty()) {
        return 0;
    }

    int i = 0;
    for (const auto &service : m_services) {
        if (service && desktopFile.endsWith(service->desktopEntryName() + QLatin1StringView(".desktop"))) {
            return i;
        }
        ++i;
    }
    return -1;
}

int VirtualKeyboardsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_services.count();
}

#include "kcmvirtualkeyboard.moc"
#include "moc_kcmvirtualkeyboard.cpp"
