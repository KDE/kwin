/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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
#include "decorationmodel.h"
#include "preview.h"
// kwin
#include <kdecorationfactory.h>
// Qt
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtGui/QApplication>
#include <QtGui/QPainter>
#include <QtGui/QStyle>
#include <QtGui/QTextDocument>
// KDE
#include <KConfigGroup>
#include <KDesktopFile>
#include <KGlobalSettings>
#include <KIcon>
#include <KLocale>
#include <KStandardDirs>
#include "kwindecoration.h"

namespace KWin
{

DecorationModel::DecorationModel(KSharedConfigPtr config, QObject* parent)
    : QAbstractListModel(parent)
    , m_plugins(new KDecorationPreviewPlugins(config))
    , m_preview(new KDecorationPreview())
    , m_customButtons(false)
    , m_leftButtons(QString())
    , m_rightButtons(QString())
    , m_renderWidget(new QWidget(0))
{
    QHash<int, QByteArray> roleNames;
    roleNames[Qt::DisplayRole] = "display";
    roleNames[DecorationModel::PixmapRole] = "preview";
    roleNames[TypeRole] = "type";
    roleNames[AuroraeNameRole] = "auroraeThemeName";
    setRoleNames(roleNames);
    m_config = KSharedConfig::openConfig("auroraerc");
    findDecorations();
}

DecorationModel::~DecorationModel()
{
    delete m_preview;
    delete m_plugins;
    delete m_renderWidget;
}

void DecorationModel::reload()
{
    m_decorations.clear();
    findDecorations();
}

// Find all theme desktop files in all 'data' dirs owned by kwin.
// And insert these into a DecorationInfo structure
void DecorationModel::findDecorations()
{
    beginResetModel();
    const QStringList dirList = KGlobal::dirs()->findDirs("data", "kwin");

    foreach (const QString & dir, dirList) {
        QDir d(dir);
        if (d.exists()) {
            foreach (const QFileInfo & fi, d.entryInfoList()) {
                const QString filename(fi.absoluteFilePath());
                if (KDesktopFile::isDesktopFile(filename)) {
                    const KDesktopFile desktopFile(filename);
                    const QString libName = desktopFile.desktopGroup().readEntry("X-KDE-Library");

                    if (!libName.isEmpty() && libName.startsWith(QLatin1String("kwin3_"))) {
                        if (libName == "kwin3_aurorae") {
                            // read the Aurorae themes
                            findAuroraeThemes();
                            continue;
                        }
                        DecorationModelData data;
                        data.name = desktopFile.readName();
                        data.libraryName = libName;
                        data.type = DecorationModelData::NativeDecoration;
                        data.borderSize = KDecorationDefines::BorderNormal;
                        metaData(data, desktopFile);
                        m_decorations.append(data);
                    }
                }
            }
        }
    }
    qSort(m_decorations.begin(), m_decorations.end(), DecorationModelData::less);
    endResetModel();
}

void DecorationModel::findAuroraeThemes()
{
    // get all desktop themes
    QStringList themes = KGlobal::dirs()->findAllResources("data",
                         "aurorae/themes/*/metadata.desktop",
                         KStandardDirs::NoDuplicates);
    foreach (const QString & theme, themes) {
        int themeSepIndex = theme.lastIndexOf('/', -1);
        QString themeRoot = theme.left(themeSepIndex);
        int themeNameSepIndex = themeRoot.lastIndexOf('/', -1);
        QString packageName = themeRoot.right(themeRoot.length() - themeNameSepIndex - 1);

        KDesktopFile df(theme);
        QString name = df.readName();
        if (name.isEmpty()) {
            name = packageName;
        }

        DecorationModelData data;
        data.name = name;
        data.libraryName = "kwin3_aurorae";
        data.type = DecorationModelData::AuroraeDecoration;
        data.auroraeName = packageName;
        KConfigGroup config(m_config, data.auroraeName);
        data.borderSize = (KDecorationDefines::BorderSize)config.readEntry< int >("BorderSize", KDecorationDefines::BorderNormal);
        data.buttonSize = (KDecorationDefines::BorderSize)config.readEntry< int >("ButtonSize", KDecorationDefines::BorderNormal);
        metaData(data, df);
        m_decorations.append(data);
    }
}

void DecorationModel::metaData(DecorationModelData& data, const KDesktopFile& df)
{
    data.comment = df.readComment();
    data.author = df.desktopGroup().readEntry("X-KDE-PluginInfo-Author", QString());
    data.email = df.desktopGroup().readEntry("X-KDE-PluginInfo-Email", QString());
    data.version = df.desktopGroup().readEntry("X-KDE-PluginInfo-Version", QString());
    data.license = df.desktopGroup().readEntry("X-KDE-PluginInfo-License", QString());
    data.website = df.desktopGroup().readEntry("X-KDE-PluginInfo-Website", QString());
}

int DecorationModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return m_decorations.count();
}

QVariant DecorationModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    switch(role) {
    case Qt::DisplayRole:
    case NameRole:
        return m_decorations[ index.row()].name;
    case LibraryNameRole:
        return m_decorations[ index.row()].libraryName;
    case PixmapRole:
        return m_decorations[ index.row()].preview;
    case TypeRole:
        return m_decorations[ index.row()].type;
    case AuroraeNameRole:
        return m_decorations[ index.row()].auroraeName;
    case PackageDescriptionRole:
        return m_decorations[ index.row()].comment;
    case PackageAuthorRole:
        return m_decorations[ index.row()].author;
    case PackageEmailRole:
        return m_decorations[ index.row()].email;
    case PackageWebsiteRole:
        return m_decorations[ index.row()].website;
    case PackageVersionRole:
        return m_decorations[ index.row()].version;
    case PackageLicenseRole:
        return m_decorations[ index.row()].license;
    case BorderSizeRole:
        return static_cast< int >(m_decorations[ index.row()].borderSize);
    case BorderSizesRole: {
        QList< QVariant > sizes;
        if (m_plugins->loadPlugin(m_decorations[ index.row()].libraryName) &&
                m_plugins->factory() != NULL) {
            foreach (KDecorationDefines::BorderSize size, m_plugins->factory()->borderSizes())   // krazy:exclude=foreach
            sizes << int(size) ;
        }
        return sizes;
    }
    case ButtonSizeRole:
        if (m_decorations[ index.row()].type == DecorationModelData::AuroraeDecoration)
            return static_cast< int >(m_decorations[ index.row()].buttonSize);
        else
            return QVariant();
    default:
        return QVariant();
    }
}

bool DecorationModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || (role != BorderSizeRole && role != ButtonSizeRole))
        return QAbstractItemModel::setData(index, value, role);

    if (role == BorderSizeRole) {
        m_decorations[ index.row()].borderSize = (KDecorationDefines::BorderSize)value.toInt();
        if (m_decorations[ index.row()].type == DecorationModelData::AuroraeDecoration) {
            KConfigGroup config(m_config, m_decorations[ index.row()].auroraeName);
            config.writeEntry("BorderSize", value.toInt());
            config.sync();
        }
        emit dataChanged(index, index);
        regeneratePreview(index);
        return true;
    }
    if (role == ButtonSizeRole && m_decorations[ index.row()].type == DecorationModelData::AuroraeDecoration) {
        m_decorations[ index.row()].buttonSize = (KDecorationDefines::BorderSize)value.toInt();
        KConfigGroup config(m_config, m_decorations[ index.row()].auroraeName);
        config.writeEntry("ButtonSize", value.toInt());
        config.sync();
        emit dataChanged(index, index);
        regeneratePreview(index);
        return true;
    }
    return QAbstractItemModel::setData(index, value, role);
}


void DecorationModel::changeButtons(const KWin::DecorationButtons *buttons)
{
    bool regenerate = (buttons->customPositions() != m_customButtons);
    if (!regenerate && buttons->customPositions())
        regenerate = (buttons->leftButtons() != m_leftButtons) || (buttons->rightButtons() != m_rightButtons);
    m_customButtons = buttons->customPositions();
    m_leftButtons = buttons->leftButtons();
    m_rightButtons = buttons->rightButtons();
    if (regenerate)
        regeneratePreviews();
}

void DecorationModel::setButtons(bool custom, const QString& left, const QString& right)
{
    m_customButtons = custom;
    m_leftButtons = left;
    m_rightButtons = right;
}

void DecorationModel::regeneratePreviews()
{
    for (int i = 0; i < m_decorations.count(); i++) {
        regeneratePreview(index(i), QSize(qobject_cast<KWinDecorationModule*>(QObject::parent())->itemWidth(), 150));
    }
}

void DecorationModel::regeneratePreview(const QModelIndex& index, const QSize& size)
{
    DecorationModelData& data = m_decorations[ index.row()];

    switch(data.type) {
    case DecorationModelData::NativeDecoration: {
        //Use a QTextDocument to layout the text
        QTextDocument document;

        QString html = QString("<strong>%1</strong>").arg(data.name);

        if (!data.author.isEmpty()) {
            QString authorCaption = i18nc("Caption to decoration preview, %1 author name",
                                        "by %1", data.author);

            html += QString("<br /><span style=\"font-size: %1pt;\">%2</span>")
                    .arg(KGlobalSettings::smallestReadableFont().pointSize())
                    .arg(authorCaption);
        }

        QColor color = QApplication::palette().brush(QPalette::Text).color();
        html = QString("<div style=\"color: %1\" align=\"center\">%2</div>").arg(color.name()).arg(html);

        document.setHtml(html);
        m_plugins->reset(KDecoration::SettingDecoration);
        if (m_plugins->loadPlugin(data.libraryName) &&
                m_preview->recreateDecoration(m_plugins))
            m_preview->enablePreview();
        else
            m_preview->disablePreview();
        m_plugins->destroyPreviousPlugin();
        m_preview->resize(size);
        m_preview->setTempButtons(m_plugins, m_customButtons, m_leftButtons, m_rightButtons);
        m_preview->setTempBorderSize(m_plugins, data.borderSize);
        data.preview = m_preview->preview(&document, m_renderWidget);
        break;
    }
    default:
        // nothing
        break;
    }
    emit dataChanged(index, index);
}

void DecorationModel::regeneratePreview(const QModelIndex& index)
{
    regeneratePreview(index, m_decorations.at(index.row()).preview.size());
}

QModelIndex DecorationModel::indexOfLibrary(const QString& libraryName) const
{
    for (int i = 0; i < m_decorations.count(); i++) {
        if (m_decorations.at(i).libraryName.compare(libraryName) == 0)
            return index(i);
    }
    return QModelIndex();
}

QModelIndex DecorationModel::indexOfName(const QString& decoName) const
{
    for (int i = 0; i < m_decorations.count(); i++) {
        if (m_decorations.at(i).name.compare(decoName) == 0)
            return index(i);
    }
    return QModelIndex();
}

QModelIndex DecorationModel::indexOfAuroraeName(const QString& auroraeName) const
{
    for (int i = 0; i < m_decorations.count(); i++) {
        const DecorationModelData& data = m_decorations.at(i);
        if (data.type == DecorationModelData::AuroraeDecoration &&
                data.auroraeName.compare(auroraeName) == 0)
            return index(i);
    }
    return QModelIndex();
}

void DecorationModel::setBorderSize(const QModelIndex& index, KDecorationDefines::BorderSize size)
{
    if (!index.isValid() || m_decorations[ index.row()].type == DecorationModelData::AuroraeDecoration)
        return;
    m_decorations[ index.row()].borderSize = size;
}

} // namespace KWin
