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

namespace KWin {
namespace Compositing {

Compositing::Compositing(QObject *parent)
    : QObject(parent)
    , m_animationSpeed(0)
    , m_windowThumbnail(0)
    , m_glScaleFilter(0)
    , m_xrScaleFilter(false)
    , m_glSwapStrategy(0)
    , m_glColorCorrection(false)
    , m_compositingType(0)
    , m_compositingEnabled(true)
    , m_changed(false)
    , m_openGLPlatformInterfaceModel(new OpenGLPlatformInterfaceModel(this))
    , m_openGLPlatformInterface(0)
    , m_windowsBlockCompositing(true)
{
    reset();
    connect(this, &Compositing::animationSpeedChanged,       this, &Compositing::changed);
    connect(this, &Compositing::windowThumbnailChanged,      this, &Compositing::changed);
    connect(this, &Compositing::glScaleFilterChanged,        this, &Compositing::changed);
    connect(this, &Compositing::xrScaleFilterChanged,        this, &Compositing::changed);
    connect(this, &Compositing::glSwapStrategyChanged,       this, &Compositing::changed);
    connect(this, &Compositing::glColorCorrectionChanged,    this, &Compositing::changed);
    connect(this, &Compositing::compositingTypeChanged,      this, &Compositing::changed);
    connect(this, &Compositing::compositingEnabledChanged,   this, &Compositing::changed);
    connect(this, &Compositing::openGLPlatformInterfaceChanged, this, &Compositing::changed);
    connect(this, &Compositing::windowsBlockCompositingChanged, this, &Compositing::changed);

    connect(this, &Compositing::changed, [this]{
        m_changed = true;
    });
}

void Compositing::reset()
{
    KConfigGroup kwinConfig(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Compositing"));
    setAnimationSpeed(kwinConfig.readEntry("AnimationSpeed", 3));
    setWindowThumbnail(kwinConfig.readEntry("HiddenPreviews", 5) - 4);
    setGlScaleFilter(kwinConfig.readEntry("GLTextureFilter", 2));
    setXrScaleFilter(kwinConfig.readEntry("XRenderSmoothScale", false));
    setCompositingEnabled(kwinConfig.readEntry("Enabled", true));

    auto swapStrategy = [&kwinConfig]() {
        const QString glSwapStrategyValue = kwinConfig.readEntry("GLPreferBufferSwap", "a");

        if (glSwapStrategyValue == "n") {
            return 0;
        } else if (glSwapStrategyValue == "a") {
            return 1;
        } else if (glSwapStrategyValue == "e") {
            return 2;
        } else if (glSwapStrategyValue == "p") {
            return 3;
        } else if (glSwapStrategyValue == "c") {
            return 4;
        }
        return 0;
    };
    setGlSwapStrategy(swapStrategy());
    setGlColorCorrection(kwinConfig.readEntry("GLColorCorrection", false));

    auto type = [&kwinConfig]{
        const QString backend = kwinConfig.readEntry("Backend", "OpenGL");
        const bool glCore = kwinConfig.readEntry("GLCore", false);

        if (backend == QStringLiteral("OpenGL")) {
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

    const QModelIndex index = m_openGLPlatformInterfaceModel->indexForKey(kwinConfig.readEntry("GLPlatformInterface", "glx"));
    setOpenGLPlatformInterface(index.isValid() ? index.row() : 0);

    setWindowsBlockCompositing(kwinConfig.readEntry("WindowsBlockCompositing", true));

    m_changed = false;
}

void Compositing::defaults()
{
    setAnimationSpeed(3);
    setWindowThumbnail(1);
    setGlScaleFilter(2);
    setXrScaleFilter(false);
    setGlSwapStrategy(1);
    setGlColorCorrection(false);
    setCompositingType(CompositingType::OPENGL20_INDEX);
    const QModelIndex index = m_openGLPlatformInterfaceModel->indexForKey(QStringLiteral("glx"));
    setOpenGLPlatformInterface(index.isValid() ? index.row() : 0);
    setWindowsBlockCompositing(true);
    m_changed = true;
}

bool Compositing::OpenGLIsUnsafe() const
{
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Compositing");
    return kwinConfig.readEntry("OpenGLIsUnsafe", true);
}

bool Compositing::OpenGLIsBroken()
{
    OrgKdeKwinCompositingInterface interface(QStringLiteral("org.kde.KWin"),
                                             QStringLiteral("/Compositor"),
                                             QDBusConnection::sessionBus());
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Compositing");

    QString oldBackend = kwinConfig.readEntry("Backend", "OpenGL");
    kwinConfig.writeEntry("Backend", "OpenGL");
    kwinConfig.sync();

    if (interface.openGLIsBroken()) {
        kwinConfig.writeEntry("Backend", oldBackend);
        kwinConfig.sync();
        return true;
    }

    kwinConfig.writeEntry("OpenGLIsUnsafe", false);
    kwinConfig.sync();
    return false;
}

void Compositing::reenableOpenGLDetection()
{
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Compositing");
    kwinConfig.writeEntry("OpenGLIsUnsafe", false);
    kwinConfig.sync();
}

int Compositing::animationSpeed() const
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

bool Compositing::glColorCorrection() const
{
    return m_glColorCorrection;
}

int Compositing::compositingType() const
{
    return m_compositingType;
}

bool Compositing::compositingEnabled() const
{
    return m_compositingEnabled;
}

void Compositing::setAnimationSpeed(int speed)
{
    if (speed == m_animationSpeed) {
        return;
    }
    m_animationSpeed = speed;
    emit animationSpeedChanged(speed);
}

void Compositing::setGlColorCorrection(bool correction)
{
    if (correction == m_glColorCorrection) {
        return;
    }
    m_glColorCorrection = correction;
    emit glColorCorrectionChanged(correction);
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
    if (enabled == m_compositingEnabled) {
        return;
    }

    m_compositingEnabled = enabled;
    emit compositingEnabledChanged(enabled);
}

void Compositing::save()
{
    KConfigGroup kwinConfig(KSharedConfig::openConfig(QStringLiteral("kwinrc")), "Compositing");
    kwinConfig.writeEntry("AnimationSpeed", animationSpeed());
    kwinConfig.writeEntry("HiddenPreviews", windowThumbnail() + 4);
    kwinConfig.writeEntry("GLTextureFilter", glScaleFilter());
    kwinConfig.writeEntry("XRenderSmoothScale", xrScaleFilter());
    kwinConfig.writeEntry("Enabled", compositingEnabled());
    auto swapStrategy = [this] {
        switch (glSwapStrategy()) {
            case 0:
                return QStringLiteral("n");
            case 2:
                return QStringLiteral("e");
            case 3:
                return QStringLiteral("p");
            case 4:
                return QStringLiteral("c");
            case 1:
            default:
                return QStringLiteral("a");
        }
    };
    kwinConfig.writeEntry("GLPreferBufferSwap", swapStrategy());
    kwinConfig.writeEntry("GLColorCorrection", glColorCorrection());
    QString backend;
    bool glCore = false;
    switch (compositingType()) {
    case CompositingType::OPENGL31_INDEX:
        backend = "OpenGL";
        glCore = true;
        break;
    case CompositingType::OPENGL20_INDEX:
        backend = "OpenGL";
        glCore = false;
        break;
    case CompositingType::XRENDER_INDEX:
        backend = "XRender";
        glCore = false;
        break;
    }
    kwinConfig.writeEntry("Backend", backend);
    kwinConfig.writeEntry("GLCore", glCore);
    const QModelIndex glIndex = m_openGLPlatformInterfaceModel->index(m_openGLPlatformInterface);
    if (glIndex.isValid()) {
        kwinConfig.writeEntry("GLPlatformInterface", glIndex.data(Qt::UserRole).toString());
    }
    kwinConfig.writeEntry("WindowsBlockCompositing", windowsBlockCompositing());
    kwinConfig.sync();

    if (m_changed) {
        // Send signal to all kwin instances
        QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/Compositor"),
                                                          QStringLiteral("org.kde.kwin.Compositing"),
                                                          QStringLiteral("reinit"));
        QDBusConnection::sessionBus().send(message);
        m_changed = false;
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
    if (m_windowsBlockCompositing == set) {
        return;
    }
    m_windowsBlockCompositing = set;
    emit windowsBlockCompositingChanged(set);
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

    qSort(m_compositingList.begin(), m_compositingList.end(), [](const CompositingData &a, const CompositingData &b) {
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
