/*
    SPDX-FileCopyrightText: 2019 Valerio Pilo <vpilo@coldshock.net>
    SPDX-FileCopyrightText: 2019 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: LGPL-2.0-only
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

#include "kwindecorationdata.h"
#include "kwindecorationsettings.h"

K_PLUGIN_FACTORY_WITH_JSON(KCMKWinDecorationFactory, "kwindecoration.json", registerPlugin<KCMKWinDecoration>();registerPlugin<KWinDecorationData>();)

Q_DECLARE_METATYPE(KDecoration2::BorderSize)


namespace
{
const KDecoration2::BorderSize s_defaultRecommendedBorderSize = KDecoration2::BorderSize::Normal;
}

KCMKWinDecoration::KCMKWinDecoration(QObject *parent, const QVariantList &arguments)
    : KQuickAddons::ManagedConfigModule(parent, arguments)
    , m_themesModel(new KDecoration2::Configuration::DecorationsModel(this))
    , m_proxyThemesModel(new QSortFilterProxyModel(this))
    , m_leftButtonsModel(new KDecoration2::Preview::ButtonsModel(DecorationButtonsList(), this))
    , m_rightButtonsModel(new KDecoration2::Preview::ButtonsModel(DecorationButtonsList(), this))
    , m_availableButtonsModel(new KDecoration2::Preview::ButtonsModel(this))
    , m_data(new KWinDecorationData(this))
{
    auto about = new KAboutData(QStringLiteral("kcm_kwindecoration"),
                                i18n("Window Decorations"),
                                QStringLiteral("1.0"),
                                QString(),
                                KAboutLicense::GPL);
    about->addAuthor(i18n("Valerio Pilo"),
                     i18n("Author"),
                     QStringLiteral("vpilo@coldshock.net"));
    setAboutData(about);
    setButtons(Apply | Default | Help);
    qmlRegisterAnonymousType<QAbstractListModel>("org.kde.kwin.KWinDecoration", 1);
    qmlRegisterAnonymousType<QSortFilterProxyModel>("org.kde.kwin.KWinDecoration", 1);
    qmlRegisterAnonymousType<KWinDecorationSettings>("org.kde.kwin.KWinDecoration", 1);
    m_proxyThemesModel->setSourceModel(m_themesModel);
    m_proxyThemesModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyThemesModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_proxyThemesModel->sort(0);

    connect(m_data->settings(), &KWinDecorationSettings::themeChanged, this, &KCMKWinDecoration::themeChanged);
    connect(m_data->settings(), &KWinDecorationSettings::borderSizeChanged, this, &KCMKWinDecoration::borderSizeChanged);

    connect(m_data->settings(), &KWinDecorationSettings::borderSizeAutoChanged, this, &KCMKWinDecoration::borderIndexChanged);
    connect(this, &KCMKWinDecoration::borderSizeChanged, this, &KCMKWinDecoration::borderIndexChanged);
    connect(this, &KCMKWinDecoration::themeChanged, this, &KCMKWinDecoration::borderIndexChanged);

    connect(this, &KCMKWinDecoration::themeChanged, this, [=](){
        if (m_data->settings()->borderSizeAuto()) {
            setBorderSize(recommendedBorderSize());
        }
    });

    connect(m_leftButtonsModel, &QAbstractItemModel::rowsInserted, this, &KCMKWinDecoration::onLeftButtonsChanged);
    connect(m_leftButtonsModel, &QAbstractItemModel::rowsMoved, this, &KCMKWinDecoration::onLeftButtonsChanged);
    connect(m_leftButtonsModel, &QAbstractItemModel::rowsRemoved, this, &KCMKWinDecoration::onLeftButtonsChanged);
    connect(m_leftButtonsModel, &QAbstractItemModel::modelReset, this, &KCMKWinDecoration::onLeftButtonsChanged);

    connect(m_rightButtonsModel, &QAbstractItemModel::rowsInserted, this, &KCMKWinDecoration::onRightButtonsChanged);
    connect(m_rightButtonsModel, &QAbstractItemModel::rowsMoved, this, &KCMKWinDecoration::onRightButtonsChanged);
    connect(m_rightButtonsModel, &QAbstractItemModel::rowsRemoved, this, &KCMKWinDecoration::onRightButtonsChanged);
    connect(m_rightButtonsModel, &QAbstractItemModel::modelReset, this, &KCMKWinDecoration::onRightButtonsChanged);

    connect(this, &KCMKWinDecoration::borderSizeChanged, this, &KCMKWinDecoration::settingsChanged);

    // Update the themes when the color scheme or a theme's settings change
    QDBusConnection::sessionBus()
        .connect(QString(), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"), QStringLiteral("reloadConfig"),
            this, SLOT(reloadKWinSettings()));

    QMetaObject::invokeMethod(m_themesModel, "init", Qt::QueuedConnection);
}

KWinDecorationSettings *KCMKWinDecoration::settings() const
{
    return m_data->settings();
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

    connect(m_newStuffDialog, &QDialog::finished, this, &KCMKWinDecoration::reloadKWinSettings);

    m_newStuffDialog->show();
}

void KCMKWinDecoration::load()
{
    ManagedConfigModule::load();

    m_leftButtonsModel->replace(Utils::buttonsFromString(settings()->buttonsOnLeft()));
    m_rightButtonsModel->replace(Utils::buttonsFromString(settings()->buttonsOnRight()));

    setBorderSize(borderSizeIndexFromString(settings()->borderSize()));

    emit themeChanged();
}

void KCMKWinDecoration::save()
{
    if (!settings()->borderSizeAuto()) {
        settings()->setBorderSize(borderSizeIndexToString(m_borderSizeIndex));
    } else {
        settings()->setBorderSize(settings()->defaultBorderSizeValue());
    }

    ManagedConfigModule::save();

    // Send a signal to all kwin instances
    QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/KWin"),
                                                      QStringLiteral("org.kde.KWin"),
                                                      QStringLiteral("reloadConfig"));
    QDBusConnection::sessionBus().send(message);
}

void KCMKWinDecoration::defaults()
{
    ManagedConfigModule::defaults();

    setBorderSize(recommendedBorderSize());

    m_leftButtonsModel->replace(Utils::buttonsFromString(settings()->buttonsOnLeft()));
    m_rightButtonsModel->replace(Utils::buttonsFromString(settings()->buttonsOnRight()));
}

void KCMKWinDecoration::onLeftButtonsChanged()
{
    settings()->setButtonsOnLeft(Utils::buttonsToString(m_leftButtonsModel->buttons()));
}

void KCMKWinDecoration::onRightButtonsChanged()
{
    settings()->setButtonsOnRight(Utils::buttonsToString(m_rightButtonsModel->buttons()));
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
    // Use index 0 for borderSizeAuto == true
    // The rest of indexes get offset by 1
    QStringList model = Utils::getBorderSizeNames().values();
    model.insert(0, i18nc("%1 is the name of a border size",
                          "Theme's default (%1)", model.at(recommendedBorderSize())));
    return model;
}

int KCMKWinDecoration::borderIndex() const
{
    return settings()->borderSizeAuto() ? 0 : m_borderSizeIndex + 1;
}

void KCMKWinDecoration::setBorderIndex(int index)
{
    const bool borderAuto = (index == 0);
    settings()->setBorderSizeAuto(borderAuto);
    setBorderSize(borderAuto ? recommendedBorderSize() : index - 1);
}

int KCMKWinDecoration::borderSize() const
{
    return m_borderSizeIndex;
}

int KCMKWinDecoration::recommendedBorderSize() const
{
    typedef KDecoration2::Configuration::DecorationsModel::DecorationRole DecoRole;
    const QModelIndex proxyIndex = m_proxyThemesModel->index(theme(), 0);
    if (proxyIndex.isValid()) {
        const QModelIndex index = m_proxyThemesModel->mapToSource(proxyIndex);
        if (index.isValid()) {
            QVariant ret = m_themesModel->data(index, DecoRole::RecommendedBorderSizeRole);
            return Utils::getBorderSizeNames().keys().indexOf(Utils::stringToBorderSize(ret.toString()));
        }
    }
    return Utils::getBorderSizeNames().keys().indexOf(s_defaultRecommendedBorderSize);
}

int KCMKWinDecoration::theme() const
{
    return m_proxyThemesModel->mapFromSource(m_themesModel->findDecoration(settings()->pluginName(), settings()->theme())).row();
}

void KCMKWinDecoration::setBorderSize(int index)
{
    if (m_borderSizeIndex != index) {
        m_borderSizeIndex = index;
        emit borderSizeChanged();
    }
}

void KCMKWinDecoration::setBorderSize(KDecoration2::BorderSize size)
{
    settings()->setBorderSize(Utils::borderSizeToString(size));
}

void KCMKWinDecoration::setTheme(int index)
{
    QModelIndex dataIndex = m_proxyThemesModel->index(index, 0);
    if (dataIndex.isValid()) {
        settings()->setTheme(m_proxyThemesModel->data(dataIndex, KDecoration2::Configuration::DecorationsModel::ThemeNameRole).toString());
        settings()->setPluginName(m_proxyThemesModel->data(dataIndex, KDecoration2::Configuration::DecorationsModel::PluginNameRole).toString());
        emit themeChanged();
    }
}

bool KCMKWinDecoration::isSaveNeeded() const
{
    return !settings()->borderSizeAuto() && borderSizeIndexFromString(settings()->borderSize()) != m_borderSizeIndex;
}

int KCMKWinDecoration::borderSizeIndexFromString(const QString &size) const
{
    return Utils::getBorderSizeNames().keys().indexOf(Utils::stringToBorderSize(size));
}

QString KCMKWinDecoration::borderSizeIndexToString(int index) const
{
    return Utils::borderSizeToString(Utils::getBorderSizeNames().keys().at(index));
}

#include "kcm.moc"
