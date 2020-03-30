/**************************************************************************
* KWin - the KDE window manager                                          *
* This file is part of the KDE project.                                  *
*                                                                        *
* Copyright (C) 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>             *
* Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>                 *
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

#include "compositing.h"
#include <kwin_compositing_interface.h>

#include <KCModuleProxy>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QDBusInterface>
#include <QDBusReply>
#include <QHash>
#include <QDebug>

#include "kwincompositing_setting.h"

namespace KWin {
namespace Compositing {

Compositing::Compositing(QObject *parent)
    : QObject(parent)
    , m_animationSpeed(1.0)
    , m_windowThumbnail(0)
    , m_glScaleFilter(0)
    , m_xrScaleFilter(false)
    , m_glSwapStrategy(0)
    , m_compositingType(0)
    , m_compositingEnabled(true)
    , m_openGLPlatformInterfaceModel(new OpenGLPlatformInterfaceModel(this))
    , m_openGLPlatformInterface(0)
    , m_windowsBlockCompositing(true)
    , m_compositingInterface(new OrgKdeKwinCompositingInterface(QStringLiteral("org.kde.KWin"), QStringLiteral("/Compositor"), QDBusConnection::sessionBus(), this))
    , m_settings(new KWinCompositingSetting(this))
{
    load();
    connect(this, &Compositing::animationSpeedChanged,       this, &Compositing::updateSettings);
    connect(this, &Compositing::windowThumbnailChanged,      this, &Compositing::updateSettings);
    connect(this, &Compositing::glScaleFilterChanged,        this, &Compositing::updateSettings);
    connect(this, &Compositing::xrScaleFilterChanged,        this, &Compositing::updateSettings);
    connect(this, &Compositing::glSwapStrategyChanged,       this, &Compositing::updateSettings);
    connect(this, &Compositing::compositingTypeChanged,      this, &Compositing::updateSettings);
    connect(this, &Compositing::compositingEnabledChanged,   this, &Compositing::updateSettings);
    connect(this, &Compositing::openGLPlatformInterfaceChanged, this, &Compositing::updateSettings);
    connect(this, &Compositing::windowsBlockCompositingChanged, this, &Compositing::updateSettings);
}

void Compositing::load()
{
    m_settings->load();

    applyValues();

    emit changed(false);
    emit defaulted(m_settings->isDefaults());
}

void Compositing::applyValues()
{
    setAnimationSpeed(m_settings->animationDurationFactor());
    // from options.cpp Options::reloadCompositingSettings
    // 4 - off, 5 - shown, 6 - always, other are old values
    setWindowThumbnail(m_settings->hiddenPreviews() - 4);
    setGlScaleFilter(m_settings->glTextureFilter());
    setXrScaleFilter(m_settings->xRenderSmoothScale());
    setCompositingEnabled(m_settings->enabled());
    setGlSwapStrategy(m_settings->glPreferBufferSwap());

    const auto type = [this]{
        const int backend = m_settings->backend();
        const bool glCore = m_settings->glCore();

        if (backend == KWinCompositingSetting::EnumBackend::OpenGL) {
            if (glCore) {
                return CompositingType::OPENGL31_INDEX;
            } else {
                return CompositingType::OPENGL20_INDEX;
            }
        } else {
            return CompositingType::XRENDER_INDEX;
        }
    };
    setCompositingType(type());

    const QModelIndex index = m_openGLPlatformInterfaceModel->indexForKey(m_settings->glPlatformInterface());
    setOpenGLPlatformInterface(index.isValid() ? index.row() : 0);

    setWindowsBlockCompositing(m_settings->windowsBlockCompositing());
}

void Compositing::defaults()
{
    m_settings->setDefaults();

    applyValues();

    emit changed(m_settings->isSaveNeeded());
    emit defaulted(m_settings->isDefaults());
}

bool Compositing::OpenGLIsUnsafe() const
{
    return m_settings->openGLIsUnsafe();
}

bool Compositing::OpenGLIsBroken()
{
    const int oldBackend = m_settings->backend();
    m_settings->setBackend(KWinCompositingSetting::EnumBackend::OpenGL);
    m_settings->save();

    if (m_compositingInterface->openGLIsBroken()) {
        m_settings->setBackend(oldBackend);
        m_settings->save();
        return true;
    }

    m_settings->setOpenGLIsUnsafe(false);
    m_settings->save();
    return false;
}

void Compositing::reenableOpenGLDetection()
{
    m_settings->setOpenGLIsUnsafe(false);
    m_settings->save();
}

qreal Compositing::animationSpeed() const
{
    return m_animationSpeed;
}

int Compositing::windowThumbnail() const
{
    return m_windowThumbnail;
}

int Compositing::glScaleFilter() const
{
    return m_glScaleFilter;
}

bool Compositing::xrScaleFilter() const
{
    return m_xrScaleFilter;
}

int Compositing::glSwapStrategy() const
{
    return m_glSwapStrategy;
}

int Compositing::compositingType() const
{
    return m_compositingType;
}

bool Compositing::compositingEnabled() const
{
    return m_compositingEnabled;
}

void Compositing::setAnimationSpeed(qreal speed)
{
    if (speed == m_animationSpeed) {
        return;
    }
    m_animationSpeed = speed;
    emit animationSpeedChanged(speed);
}

void Compositing::setGlScaleFilter(int index)
{
    if (index == m_glScaleFilter) {
        return;
    }
    m_glScaleFilter = index;
    emit glScaleFilterChanged(index);
}

void Compositing::setGlSwapStrategy(int strategy)
{
    if (strategy == m_glSwapStrategy) {
        return;
    }
    m_glSwapStrategy = strategy;
    emit glSwapStrategyChanged(strategy);
}

void Compositing::setWindowThumbnail(int index)
{
    if (index == m_windowThumbnail) {
        return;
    }
    m_windowThumbnail = index;
    emit windowThumbnailChanged(index);
}

void Compositing::setXrScaleFilter(bool filter)
{
    if (filter == m_xrScaleFilter) {
        return;
    }
    m_xrScaleFilter = filter;
    emit xrScaleFilterChanged(filter);
}

void Compositing::setCompositingType(int index)
{
    if (index == m_compositingType) {
        return;
    }
    m_compositingType = index;
    emit compositingTypeChanged(index);
}

void Compositing::setCompositingEnabled(bool enabled)
{
    if (compositingRequired()) {
        return;
    }
    if (enabled == m_compositingEnabled) {
        return;
    }

    m_compositingEnabled = enabled;
    emit compositingEnabledChanged(enabled);
}

void Compositing::updateSettings()
{
    // this writes to the KDE group of the kwinrc, when loading we rely on kconfig cascading to
    // load a global value, or allow a kwin override
    if (!isRunningPlasma()) {
        m_settings->setAnimationDurationFactor(animationSpeed());
    }

    m_settings->setHiddenPreviews(windowThumbnail() + 4);
    m_settings->setGlTextureFilter(glScaleFilter());
    m_settings->setXRenderSmoothScale(xrScaleFilter());
    if (!compositingRequired()) {
        m_settings->setEnabled(compositingEnabled());
    }
    m_settings->setGlPreferBufferSwap(glSwapStrategy());
    int backend = KWinCompositingSetting::EnumBackend::OpenGL;
    bool glCore = false;
    switch (compositingType()) {
    case CompositingType::OPENGL31_INDEX:
        backend = KWinCompositingSetting::EnumBackend::OpenGL;
        glCore = true;
        break;
    case CompositingType::OPENGL20_INDEX:
        backend = KWinCompositingSetting::EnumBackend::OpenGL;
        glCore = false;
        break;
    case CompositingType::XRENDER_INDEX:
        backend = KWinCompositingSetting::EnumBackend::XRender;
        glCore = false;
        break;
    }
    m_settings->setBackend(backend);
    m_settings->setGlCore(glCore);
    if (!compositingRequired()) {
        m_settings->setWindowsBlockCompositing(windowsBlockCompositing());
    }

    emit changed(m_settings->isSaveNeeded());
    emit defaulted(m_settings->isDefaults());
}

void Compositing::save()
{
    if (m_settings->isSaveNeeded()) {

        m_settings->save();

        // Send signal to all kwin instances
        QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/Compositor"),
                                                          QStringLiteral("org.kde.kwin.Compositing"),
                                                          QStringLiteral("reinit"));
        QDBusConnection::sessionBus().send(message);
    }
}

OpenGLPlatformInterfaceModel *Compositing::openGLPlatformInterfaceModel() const
{
    return m_openGLPlatformInterfaceModel;
}

int Compositing::openGLPlatformInterface() const
{
    return m_openGLPlatformInterface;
}

void Compositing::setOpenGLPlatformInterface(int interface)
{
    if (m_openGLPlatformInterface == interface) {
        return;
    }
    m_openGLPlatformInterface = interface;
    emit openGLPlatformInterfaceChanged(interface);
}

bool Compositing::windowsBlockCompositing() const
{
    return m_windowsBlockCompositing;
}

void Compositing::setWindowsBlockCompositing(bool set)
{
    if (compositingRequired()) {
        return;
    }
    if (m_windowsBlockCompositing == set) {
        return;
    }
    m_windowsBlockCompositing = set;
    emit windowsBlockCompositingChanged(set);
}

bool Compositing::compositingRequired() const
{
    return m_compositingInterface->platformRequiresCompositing();
}

bool Compositing::isRunningPlasma()
{
    return qgetenv("XDG_CURRENT_DESKTOP") == "KDE";
}

CompositingType::CompositingType(QObject *parent)
    : QAbstractItemModel(parent) {

    generateCompositing();
}

void CompositingType::generateCompositing()
{
    QHash<QString, CompositingType::CompositingTypeIndex> compositingTypes;

    compositingTypes[i18n("OpenGL 3.1")] = CompositingType::OPENGL31_INDEX;
    compositingTypes[i18n("OpenGL 2.0")] = CompositingType::OPENGL20_INDEX;
    compositingTypes[i18n("XRender")] = CompositingType::XRENDER_INDEX;

    CompositingData data;
    beginResetModel();
    auto it = compositingTypes.begin();
    while (it != compositingTypes.end()) {
        data.name = it.key();
        data.type = it.value();
        m_compositingList << data;
        it++;
    }

    std::sort(m_compositingList.begin(), m_compositingList.end(), [](const CompositingData &a, const CompositingData &b) {
        return a.type < b.type;
    });
    endResetModel();
}

QHash< int, QByteArray > CompositingType::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[NameRole] = "NameRole";
    roleNames[TypeRole] = QByteArrayLiteral("type");
    return roleNames;
}

QModelIndex CompositingType::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() || column > 0 || column < 0 || row < 0 || row >= m_compositingList.count()) {
        return QModelIndex();
    }

    return createIndex(row, column);
}

QModelIndex CompositingType::parent(const QModelIndex &child) const
{
    Q_UNUSED(child)

    return QModelIndex();
}

int CompositingType::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

int CompositingType::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_compositingList.count();
}

QVariant CompositingType::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    switch (role) {
        case Qt::DisplayRole:
        case NameRole:
            return m_compositingList.at(index.row()).name;
        case TypeRole:
            return m_compositingList.at(index.row()).type;
        default:
            return QVariant();
    }
}

int CompositingType::compositingTypeForIndex(int row) const
{
    return index(row, 0).data(TypeRole).toInt();
}

int CompositingType::indexForCompositingType(int type) const
{
    for (int i = 0; i < m_compositingList.count(); ++i) {
        if (m_compositingList.at(i).type == type) {
            return i;
        }
    }
    return -1;
}

OpenGLPlatformInterfaceModel::OpenGLPlatformInterfaceModel(QObject *parent)
    : QAbstractListModel(parent)
{
    beginResetModel();
    OrgKdeKwinCompositingInterface interface(QStringLiteral("org.kde.KWin"),
                                             QStringLiteral("/Compositor"),
                                             QDBusConnection::sessionBus());
    m_keys << interface.supportedOpenGLPlatformInterfaces();
    for (const QString &key : m_keys) {
        if (key == QStringLiteral("egl")) {
            m_names << i18nc("OpenGL Platform Interface", "EGL");
        } else if (key == QStringLiteral("glx")) {
            m_names << i18nc("OpenGL Platform Interface", "GLX");
        } else {
            m_names << key;
        }
    }
    endResetModel();
}

OpenGLPlatformInterfaceModel::~OpenGLPlatformInterfaceModel() = default;

int OpenGLPlatformInterfaceModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_keys.count();
}

QHash< int, QByteArray > OpenGLPlatformInterfaceModel::roleNames() const
{
    return QHash<int, QByteArray>({
        {Qt::DisplayRole, QByteArrayLiteral("display")},
        {Qt::UserRole, QByteArrayLiteral("openglPlatformInterface")}
    });
}

QVariant OpenGLPlatformInterfaceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_keys.size() || index.column() != 0) {
        return QVariant();
    }
    switch (role) {
    case Qt::DisplayRole:
        return m_names.at(index.row());
    case Qt::UserRole:
        return m_keys.at(index.row());
    default:
        return QVariant();
    }
}

QModelIndex OpenGLPlatformInterfaceModel::indexForKey(const QString &key) const
{
    const int keyIndex = m_keys.indexOf(key);
    if (keyIndex < 0) {
        return QModelIndex();
    }
    return createIndex(keyIndex, 0);
}

}//end namespace Compositing
}//end namespace KWin
