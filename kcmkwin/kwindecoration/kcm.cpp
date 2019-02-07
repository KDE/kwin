/*
 * Copyright (c) 2019 Valerio Pilo <vpilo@coldshock.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "kcm.h"
#include "decorationmodel.h"
#include "declarative-plugin/buttonsmodel.h"
#include <config-kwin.h>

#include <KAboutData>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KNSCore/Engine>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSortFilterProxyModel>

#include <KNewStuff3/KNS3/DownloadDialog>


K_PLUGIN_FACTORY_WITH_JSON(KCMKWinDecorationFactory, "kwindecoration.json", registerPlugin<KCMKWinDecoration>();)

Q_DECLARE_METATYPE(KDecoration2::BorderSize)


namespace
{
const QString s_configFile { QStringLiteral("kwinrc") };
const QString s_configGroup { QStringLiteral("org.kde.kdecoration2") };
const QString s_configPlugin { QStringLiteral("library") };
const QString s_configTheme { QStringLiteral("theme") };
const QString s_configBorderSize { QStringLiteral("BorderSize") };
const QString s_configCloseOnDoubleClickOnMenu { QStringLiteral("CloseOnDoubleClickOnMenu") };
const QString s_configDecoButtonsOnLeft { QStringLiteral("ButtonsOnLeft") };
const QString s_configDecoButtonsOnRight { QStringLiteral("ButtonsOnRight") };

const KDecoration2::BorderSize s_defaultBorderSize = KDecoration2::BorderSize::Normal;
const bool s_defaultCloseOnDoubleClickOnMenu = false;

const DecorationButtonsList s_defaultDecoButtonsOnLeft {
    KDecoration2::DecorationButtonType::Menu,
    KDecoration2::DecorationButtonType::OnAllDesktops
};
const DecorationButtonsList s_defaultDecoButtonsOnRight {
    KDecoration2::DecorationButtonType::ContextHelp,
    KDecoration2::DecorationButtonType::Minimize,
    KDecoration2::DecorationButtonType::Maximize,
    KDecoration2::DecorationButtonType::Close
};

#if HAVE_BREEZE_DECO
const QString s_defaultPlugin { QStringLiteral(BREEZE_KDECORATION_PLUGIN_ID) };
const QString s_defaultTheme  { QStringLiteral("Breeze") };
#else
const QString s_defaultPlugin { QStringLiteral("org.kde.kwin.aurorae") };
const QString s_defaultTheme  { QStringLiteral("kwin4_decoration_qml_plastik") };
#endif
}

KCMKWinDecoration::KCMKWinDecoration(QObject *parent, const QVariantList &arguments)
    : KQuickAddons::ConfigModule(parent, arguments)
    , m_themesModel(new KDecoration2::Configuration::DecorationsModel(this))
    , m_proxyThemesModel(new QSortFilterProxyModel(this))
    , m_leftButtonsModel(new KDecoration2::Preview::ButtonsModel(DecorationButtonsList(), this))
    , m_rightButtonsModel(new KDecoration2::Preview::ButtonsModel(DecorationButtonsList(), this))
    , m_availableButtonsModel(new KDecoration2::Preview::ButtonsModel(this))
    , m_savedSettings{ s_defaultBorderSize, -2 /* for setTheme() */, false, s_defaultDecoButtonsOnLeft, s_defaultDecoButtonsOnRight }
    , m_currentSettings(m_savedSettings)
{
    auto about = new KAboutData(QStringLiteral("kcm_kwindecoration"),
                                i18n("Configure window titlebars and borders"),
                                QStringLiteral("1.0"),
                                QString(),
                                KAboutLicense::GPL);
    about->addAuthor(i18n("Valerio Pilo"),
                     i18n("Author"),
                     QStringLiteral("vpilo@coldshock.net"));
    setAboutData(about);

    qmlRegisterType<QAbstractListModel>();
    qmlRegisterType<QSortFilterProxyModel>();

    m_proxyThemesModel->setSourceModel(m_themesModel);
    m_proxyThemesModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyThemesModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_proxyThemesModel->sort(0);

    connect(m_leftButtonsModel, &QAbstractItemModel::rowsInserted, this, &KCMKWinDecoration::updateNeedsSave);
    connect(m_leftButtonsModel, &QAbstractItemModel::rowsMoved, this, &KCMKWinDecoration::updateNeedsSave);
    connect(m_leftButtonsModel, &QAbstractItemModel::rowsRemoved, this, &KCMKWinDecoration::updateNeedsSave);
    connect(m_leftButtonsModel, &QAbstractItemModel::modelReset, this, &KCMKWinDecoration::updateNeedsSave);
    connect(m_rightButtonsModel, &QAbstractItemModel::rowsInserted, this, &KCMKWinDecoration::updateNeedsSave);
    connect(m_rightButtonsModel, &QAbstractItemModel::rowsMoved, this, &KCMKWinDecoration::updateNeedsSave);
    connect(m_rightButtonsModel, &QAbstractItemModel::rowsRemoved, this, &KCMKWinDecoration::updateNeedsSave);
    connect(m_rightButtonsModel, &QAbstractItemModel::modelReset, this, &KCMKWinDecoration::updateNeedsSave);

    // Update the themes when the color scheme or a theme's settings change
    QDBusConnection::sessionBus()
        .connect(QString(), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"), QStringLiteral("reloadConfig"),
            this,
            SLOT(reloadKWinSettings()));

    QMetaObject::invokeMethod(m_themesModel, "init", Qt::QueuedConnection);
}

void KCMKWinDecoration::reloadKWinSettings()
{
    QMetaObject::invokeMethod(m_themesModel, "init", Qt::QueuedConnection);
}

void KCMKWinDecoration::getNewStuff(QQuickItem *context)
{
    if (!m_newStuffDialog) {
        m_newStuffDialog = new KNS3::DownloadDialog(QStringLiteral("window-decorations.knsrc"));
        m_newStuffDialog->setWindowTitle(i18n("Download New Window Decorations"));
        m_newStuffDialog->setWindowModality(Qt::WindowModal);
        connect(m_newStuffDialog, &KNS3::DownloadDialog::accepted, this, &KCMKWinDecoration::load);
    }

    if (context && context->window()) {
        m_newStuffDialog->winId(); // so it creates the windowHandle()
        m_newStuffDialog->windowHandle()->setTransientParent(context->window());
    }

    m_newStuffDialog->show();
}

void KCMKWinDecoration::load()
{
    const KConfigGroup config = KSharedConfig::openConfig(s_configFile)->group(s_configGroup);

    const QString plugin = config.readEntry(s_configPlugin, s_defaultPlugin);
    const QString theme = config.readEntry(s_configTheme, s_defaultTheme);
    int themeIndex = m_proxyThemesModel->mapFromSource(m_themesModel->findDecoration(plugin, theme)).row();
    if (themeIndex < 0) {
        qWarning() << "Plugin" << plugin << "and theme" << theme << "not found";
    } else {
        qDebug() << "Current theme: plugin" << plugin << "and theme" << theme;
    }
    setTheme(themeIndex);

    setCloseOnDoubleClickOnMenu(config.readEntry(s_configCloseOnDoubleClickOnMenu, s_defaultCloseOnDoubleClickOnMenu));

    const QString defaultSizeName = Utils::borderSizeToString(s_defaultBorderSize);
    setBorderSize(Utils::stringToBorderSize(config.readEntry(s_configBorderSize, defaultSizeName)));

    m_leftButtonsModel->replace(Utils::readDecorationButtons(config, s_configDecoButtonsOnLeft, s_defaultDecoButtonsOnLeft));
    m_rightButtonsModel->replace(Utils::readDecorationButtons(config, s_configDecoButtonsOnRight, s_defaultDecoButtonsOnRight));
    m_currentSettings.buttonsOnLeft = m_leftButtonsModel->buttons();
    m_currentSettings.buttonsOnRight = m_rightButtonsModel->buttons();

    m_savedSettings = m_currentSettings;

    updateNeedsSave();
}

void KCMKWinDecoration::save()
{
    KConfigGroup config = KSharedConfig::openConfig(s_configFile)->group(s_configGroup);

    if (m_currentSettings.themeIndex >= 0) {
        const QModelIndex index = m_proxyThemesModel->index(m_currentSettings.themeIndex, 0);
        if (index.isValid()) {
            const QString plugin = index.data(KDecoration2::Configuration::DecorationsModel::PluginNameRole).toString();
            const QString theme = index.data(KDecoration2::Configuration::DecorationsModel::ThemeNameRole).toString();
            config.writeEntry(s_configPlugin, plugin);
            config.writeEntry(s_configTheme, theme);
            qDebug() << "Saved theme: plugin" << plugin << "and theme" << theme;
        } else {
            qWarning() << "Cannot match theme index" << m_currentSettings.themeIndex << "in model";
        }
    }

    config.writeEntry(s_configCloseOnDoubleClickOnMenu, m_currentSettings.closeOnDoubleClickOnMenu);
    config.writeEntry(s_configBorderSize, Utils::borderSizeToString(m_currentSettings.borderSize));
    config.writeEntry(s_configDecoButtonsOnLeft, Utils::buttonsToString(m_currentSettings.buttonsOnLeft));
    config.writeEntry(s_configDecoButtonsOnRight, Utils::buttonsToString(m_currentSettings.buttonsOnRight));
    config.sync();

    m_savedSettings = m_currentSettings;

    // Send a signal to all kwin instances
    QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/KWin"),
                                                      QStringLiteral("org.kde.KWin"),
                                                      QStringLiteral("reloadConfig"));
    QDBusConnection::sessionBus().send(message);

    updateNeedsSave();
}

void KCMKWinDecoration::defaults()
{
    int themeIndex = m_proxyThemesModel->mapFromSource(m_themesModel->findDecoration(s_defaultPlugin, s_defaultTheme)).row();
    if (themeIndex < 0) {
        qWarning() << "Default plugin" << s_defaultPlugin << "and theme" << s_defaultTheme << "not found";
    }
    setTheme(themeIndex);
    setBorderSize(s_defaultBorderSize);
    setCloseOnDoubleClickOnMenu(s_defaultCloseOnDoubleClickOnMenu);

    m_leftButtonsModel->replace(s_defaultDecoButtonsOnLeft);
    m_rightButtonsModel->replace(s_defaultDecoButtonsOnRight);

    updateNeedsSave();
}

void KCMKWinDecoration::updateNeedsSave()
{
    m_currentSettings.buttonsOnLeft = m_leftButtonsModel->buttons();
    m_currentSettings.buttonsOnRight = m_rightButtonsModel->buttons();

    setNeedsSave(m_savedSettings.closeOnDoubleClickOnMenu != m_currentSettings.closeOnDoubleClickOnMenu
                || m_savedSettings.borderSize != m_currentSettings.borderSize
                || m_savedSettings.themeIndex != m_currentSettings.themeIndex
                || m_savedSettings.buttonsOnLeft != m_currentSettings.buttonsOnLeft
                || m_savedSettings.buttonsOnRight != m_currentSettings.buttonsOnRight);
}

QSortFilterProxyModel *KCMKWinDecoration::themesModel() const
{
    return m_proxyThemesModel;
}

QAbstractListModel *KCMKWinDecoration::leftButtonsModel()
{
    return m_leftButtonsModel;
}

QAbstractListModel *KCMKWinDecoration::rightButtonsModel()
{
    return m_rightButtonsModel;
}

QAbstractListModel *KCMKWinDecoration::availableButtonsModel() const
{
    return m_availableButtonsModel;
}

QStringList KCMKWinDecoration::borderSizesModel() const
{
    return Utils::getBorderSizeNames().values();
}

int KCMKWinDecoration::borderSize() const
{
    return Utils::getBorderSizeNames().keys().indexOf(m_currentSettings.borderSize);
}

int KCMKWinDecoration::theme() const
{
    return m_currentSettings.themeIndex;
}

bool KCMKWinDecoration::closeOnDoubleClickOnMenu() const
{
    return m_currentSettings.closeOnDoubleClickOnMenu;
}

void KCMKWinDecoration::setBorderSize(int index)
{
    setBorderSize(Utils::getBorderSizeNames().keys().at(index));
}

void KCMKWinDecoration::setBorderSize(KDecoration2::BorderSize size)
{
    if (m_currentSettings.borderSize == size) {
        return;
    }
    m_currentSettings.borderSize = size;
    emit borderSizeChanged();
    updateNeedsSave();
}

void KCMKWinDecoration::setTheme(int index)
{
    // The initial themeIndex is set to -2 to always initially apply a theme, any theme
    if (m_currentSettings.themeIndex == index) {
        return;
    }
    m_currentSettings.themeIndex = index;
    emit themeChanged();
    updateNeedsSave();
}

void KCMKWinDecoration::setCloseOnDoubleClickOnMenu(bool enable)
{
    if (m_currentSettings.closeOnDoubleClickOnMenu == enable) {
        return;
    }
    m_currentSettings.closeOnDoubleClickOnMenu = enable;
    emit closeOnDoubleClickOnMenuChanged();
    updateNeedsSave();
}

#include "kcm.moc"
