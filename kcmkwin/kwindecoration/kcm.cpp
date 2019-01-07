/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "kcm.h"
#include "decorationmodel.h"
#include "declarative-plugin/buttonsmodel.h"
#include <config-kwin.h>

// KDE
#include <KConfigGroup>
#include <KPluginFactory>
#include <KSharedConfig>
#include <KDecoration2/DecorationButton>
#include <KNewStuff3/KNS3/DownloadDialog>
#include <kdeclarative/kdeclarative.h>
// Qt
#include <QDBusConnection>
#include <QDBusMessage>
#include <QFontDatabase>
#include <QMenu>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QVBoxLayout>

K_PLUGIN_FACTORY(KDecorationFactory,
                 registerPlugin<KDecoration2::Configuration::ConfigurationModule>();
                )

Q_DECLARE_METATYPE(KDecoration2::BorderSize)

namespace KDecoration2
{

namespace Configuration
{
static const QString s_pluginName = QStringLiteral("org.kde.kdecoration2");
#if HAVE_BREEZE_DECO
static const QString s_defaultPlugin = QStringLiteral(BREEZE_KDECORATION_PLUGIN_ID);
static const QString s_defaultTheme;
#else
static const QString s_defaultPlugin = QStringLiteral("org.kde.kwin.aurorae");
static const QString s_defaultTheme = QStringLiteral("kwin4_decoration_qml_plastik");
#endif
static const QString s_borderSizeNormal = QStringLiteral("Normal");
static const QString s_ghnsIcon = QStringLiteral("get-hot-new-stuff");

ConfigurationForm::ConfigurationForm(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
}

static bool s_loading = false;

ConfigurationModule::ConfigurationModule(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , m_model(new DecorationsModel(this))
    , m_proxyModel(new QSortFilterProxyModel(this))
    , m_ui(new ConfigurationForm(this))
    , m_leftButtons(new Preview::ButtonsModel(QVector<DecorationButtonType>(), this))
    , m_rightButtons(new Preview::ButtonsModel(QVector<DecorationButtonType>(), this))
    , m_availableButtons(new Preview::ButtonsModel(this))
{
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->sort(0);
    connect(m_ui->filter, &QLineEdit::textChanged, m_proxyModel, &QSortFilterProxyModel::setFilterFixedString);

    m_quickView = new QQuickView(0);
    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(m_quickView->engine());
    kdeclarative.setTranslationDomain(QStringLiteral(TRANSLATION_DOMAIN));
    kdeclarative.setupContext();
    kdeclarative.setupEngine(m_quickView->engine());

    qmlRegisterType<QAbstractItemModel>();
    QWidget *widget = QWidget::createWindowContainer(m_quickView, this);
    QVBoxLayout* layout = new QVBoxLayout(m_ui->view);
    layout->setContentsMargins(0,0,0,0);
    layout->addWidget(widget);

    m_quickView->rootContext()->setContextProperty(QStringLiteral("decorationsModel"), m_proxyModel);
    updateColors();
    m_quickView->rootContext()->setContextProperty("_borderSizesIndex", 3); // 3 is normal
    m_quickView->rootContext()->setContextProperty("leftButtons", m_leftButtons);
    m_quickView->rootContext()->setContextProperty("rightButtons", m_rightButtons);
    m_quickView->rootContext()->setContextProperty("availableButtons", m_availableButtons);
    m_quickView->rootContext()->setContextProperty("initialThemeIndex", -1);

    m_quickView->rootContext()->setContextProperty("titleFont", QFontDatabase::systemFont(QFontDatabase::TitleFont));
    m_quickView->setResizeMode(QQuickView::SizeRootObjectToView);
    m_quickView->setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/kcm_kwindecoration/main.qml"))));
    if (m_quickView->status() == QQuickView::Ready) {
        auto listView = m_quickView->rootObject()->findChild<QQuickItem*>("listView");
        if (listView) {
            connect(listView, SIGNAL(userChangedSelection()), this, SLOT(changed()));
        }
    }

    m_ui->tabWidget->tabBar()->disconnect();
    auto setCurrentTab = [this](int index) {
        if (index == 0)
            m_ui->doubleClickMessage->hide();
        m_ui->filter->setVisible(index == 0);
        m_ui->knsButton->setVisible(index == 0);
        if (auto themeList = m_quickView->rootObject()->findChild<QQuickItem*>("themeList")) {
            themeList->setVisible(index == 0);
        }
        m_ui->borderSizesLabel->setVisible(index == 0);
        m_ui->borderSizesCombo->setVisible(index == 0);

        m_ui->closeWindowsDoubleClick->setVisible(index == 1);
        if (auto buttonLayout = m_quickView->rootObject()->findChild<QQuickItem*>("buttonLayout")) {
            buttonLayout->setVisible(index == 1);
        }
    };
    connect(m_ui->tabWidget->tabBar(), &QTabBar::currentChanged, this, setCurrentTab);
    setCurrentTab(0);

    m_ui->doubleClickMessage->setVisible(false);
    m_ui->doubleClickMessage->setText(i18n("Close by double clicking:\n To open the menu, keep the button pressed until it appears."));
    m_ui->doubleClickMessage->setCloseButtonVisible(true);
    m_ui->borderSizesCombo->setItemData(0, QVariant::fromValue(BorderSize::None));
    m_ui->borderSizesCombo->setItemData(1, QVariant::fromValue(BorderSize::NoSides));
    m_ui->borderSizesCombo->setItemData(2, QVariant::fromValue(BorderSize::Tiny));
    m_ui->borderSizesCombo->setItemData(3, QVariant::fromValue(BorderSize::Normal));
    m_ui->borderSizesCombo->setItemData(4, QVariant::fromValue(BorderSize::Large));
    m_ui->borderSizesCombo->setItemData(5, QVariant::fromValue(BorderSize::VeryLarge));
    m_ui->borderSizesCombo->setItemData(6, QVariant::fromValue(BorderSize::Huge));
    m_ui->borderSizesCombo->setItemData(7, QVariant::fromValue(BorderSize::VeryHuge));
    m_ui->borderSizesCombo->setItemData(8, QVariant::fromValue(BorderSize::Oversized));
    m_ui->knsButton->setIcon(QIcon::fromTheme(s_ghnsIcon));

    auto changedSlot = static_cast<void (ConfigurationModule::*)()>(&ConfigurationModule::changed);
    connect(m_ui->closeWindowsDoubleClick, &QCheckBox::stateChanged, this, changedSlot);
    connect(m_ui->closeWindowsDoubleClick, &QCheckBox::toggled, this,
        [this] (bool toggled) {
            if (s_loading) {
                return;
            }
            if (toggled)
                m_ui->doubleClickMessage->animatedShow();
            else
                m_ui->doubleClickMessage->animatedHide();
        }
    );
    connect(m_ui->borderSizesCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this] (int index) {
            auto listView = m_quickView->rootObject()->findChild<QQuickItem*>("listView");
            if (listView) {
                listView->setProperty("borderSizesIndex", index);
            }
            changed();
        }
    );
    connect(m_model, &QAbstractItemModel::modelReset, this,
        [this] {
            const auto &kns = m_model->knsProviders();
            m_ui->knsButton->setEnabled(!kns.isEmpty());
            if (kns.isEmpty()) {
                return;
            }
            if (kns.count() > 1) {
                QMenu *menu = new QMenu(m_ui->knsButton);
                for (auto it = kns.begin(); it != kns.end(); ++it) {
                    QAction *action = menu->addAction(QIcon::fromTheme(s_ghnsIcon), it.value());
                    action->setData(it.key());
                    connect(action, &QAction::triggered, this, [this, action] { showKNS(action->data().toString());});
                }
                m_ui->knsButton->setMenu(menu);
            }
        }
    );
    connect(m_ui->knsButton, &QPushButton::clicked, this,
        [this] {
            const auto &kns = m_model->knsProviders();
            if (kns.isEmpty()) {
                return;
            }
            showKNS(kns.firstKey());
        }
    );
    connect(m_leftButtons, &QAbstractItemModel::rowsInserted, this, changedSlot);
    connect(m_leftButtons, &QAbstractItemModel::rowsMoved, this, changedSlot);
    connect(m_leftButtons, &QAbstractItemModel::rowsRemoved, this, changedSlot);
    connect(m_rightButtons, &QAbstractItemModel::rowsInserted, this, changedSlot);
    connect(m_rightButtons, &QAbstractItemModel::rowsMoved, this, changedSlot);
    connect(m_rightButtons, &QAbstractItemModel::rowsRemoved, this, changedSlot);

    QVBoxLayout *l = new QVBoxLayout(this);
    l->addWidget(m_ui);
    QMetaObject::invokeMethod(m_model, "init", Qt::QueuedConnection);

    m_ui->installEventFilter(this);
}

ConfigurationModule::~ConfigurationModule() = default;

void ConfigurationModule::showEvent(QShowEvent *ev)
{
    KCModule::showEvent(ev);
}

static const QMap<QString, KDecoration2::BorderSize> s_sizes = QMap<QString, KDecoration2::BorderSize>({
    {QStringLiteral("None"), BorderSize::None},
    {QStringLiteral("NoSides"), BorderSize::NoSides},
    {QStringLiteral("Tiny"), BorderSize::Tiny},
    {s_borderSizeNormal, BorderSize::Normal},
    {QStringLiteral("Large"), BorderSize::Large},
    {QStringLiteral("VeryLarge"), BorderSize::VeryLarge},
    {QStringLiteral("Huge"), BorderSize::Huge},
    {QStringLiteral("VeryHuge"), BorderSize::VeryHuge},
    {QStringLiteral("Oversized"), BorderSize::Oversized}
});

static BorderSize stringToSize(const QString &name)
{
    auto it = s_sizes.constFind(name);
    if (it == s_sizes.constEnd()) {
        // non sense values are interpreted just like normal
        return BorderSize::Normal;
    }
    return it.value();
}

static QString sizeToString(BorderSize size)
{
    return s_sizes.key(size, s_borderSizeNormal);
}

static QHash<KDecoration2::DecorationButtonType, QChar> s_buttonNames;
static void initButtons()
{
    if (!s_buttonNames.isEmpty()) {
        return;
    }
    s_buttonNames[KDecoration2::DecorationButtonType::Menu]            = QChar('M');
    s_buttonNames[KDecoration2::DecorationButtonType::ApplicationMenu] = QChar('N');
    s_buttonNames[KDecoration2::DecorationButtonType::OnAllDesktops]   = QChar('S');
    s_buttonNames[KDecoration2::DecorationButtonType::ContextHelp]     = QChar('H');
    s_buttonNames[KDecoration2::DecorationButtonType::Minimize]        = QChar('I');
    s_buttonNames[KDecoration2::DecorationButtonType::Maximize]        = QChar('A');
    s_buttonNames[KDecoration2::DecorationButtonType::Close]           = QChar('X');
    s_buttonNames[KDecoration2::DecorationButtonType::KeepAbove]       = QChar('F');
    s_buttonNames[KDecoration2::DecorationButtonType::KeepBelow]       = QChar('B');
    s_buttonNames[KDecoration2::DecorationButtonType::Shade]           = QChar('L');
}

static QString buttonsToString(const QVector<KDecoration2::DecorationButtonType> &buttons)
{
    auto buttonToString = [](KDecoration2::DecorationButtonType button) -> QChar {
        const auto it = s_buttonNames.constFind(button);
        if (it != s_buttonNames.constEnd()) {
            return it.value();
        }
        return QChar();
    };
    QString ret;
    for (auto button : buttons) {
        ret.append(buttonToString(button));
    }
    return ret;
}

static
QVector< KDecoration2::DecorationButtonType > readDecorationButtons(const KConfigGroup &config,
                                                                    const char *key,
                                                                    const QVector< KDecoration2::DecorationButtonType > &defaultValue)
{
    initButtons();
    auto buttonsFromString = [](const QString &buttons) -> QVector<KDecoration2::DecorationButtonType> {
        QVector<KDecoration2::DecorationButtonType> ret;
        for (auto it = buttons.begin(); it != buttons.end(); ++it) {
            for (auto it2 = s_buttonNames.constBegin(); it2 != s_buttonNames.constEnd(); ++it2) {
                if (it2.value() == (*it)) {
                    ret << it2.key();
                }
            }
        }
        return ret;
    };
    return buttonsFromString(config.readEntry(key, buttonsToString(defaultValue)));
}

void ConfigurationModule::load()
{
    s_loading = true;
    const KConfigGroup config = KSharedConfig::openConfig("kwinrc")->group(s_pluginName);
    const QString plugin = config.readEntry("library", s_defaultPlugin);
    const QString theme = config.readEntry("theme", s_defaultTheme);
    m_ui->closeWindowsDoubleClick->setChecked(config.readEntry("CloseOnDoubleClickOnMenu", false));
    const QVariant border = QVariant::fromValue(stringToSize(config.readEntry("BorderSize", s_borderSizeNormal)));
    m_ui->borderSizesCombo->setCurrentIndex(m_ui->borderSizesCombo->findData(border));

    int themeIndex = m_proxyModel->mapFromSource(m_model->findDecoration(plugin, theme)).row();
    m_quickView->rootContext()->setContextProperty("initialThemeIndex", themeIndex);

    // buttons
    const auto &left = readDecorationButtons(config, "ButtonsOnLeft", QVector<KDecoration2::DecorationButtonType >{
        KDecoration2::DecorationButtonType::Menu,
        KDecoration2::DecorationButtonType::OnAllDesktops
    });
    while (m_leftButtons->rowCount() > 0) {
        m_leftButtons->remove(0);
    }
    for (auto it = left.begin(); it != left.end(); ++it) {
        m_leftButtons->add(*it);
    }
    const auto &right = readDecorationButtons(config, "ButtonsOnRight", QVector<KDecoration2::DecorationButtonType >{
        KDecoration2::DecorationButtonType::ContextHelp,
        KDecoration2::DecorationButtonType::Minimize,
        KDecoration2::DecorationButtonType::Maximize,
        KDecoration2::DecorationButtonType::Close
    });
    while (m_rightButtons->rowCount() > 0) {
        m_rightButtons->remove(0);
    }
    for (auto it = right.begin(); it != right.end(); ++it) {
        m_rightButtons->add(*it);
    }

    KCModule::load();
    s_loading = false;
}

void ConfigurationModule::save()
{
    KConfigGroup config = KSharedConfig::openConfig("kwinrc")->group(s_pluginName);
    config.writeEntry("CloseOnDoubleClickOnMenu", m_ui->closeWindowsDoubleClick->isChecked());
    config.writeEntry("BorderSize", sizeToString(m_ui->borderSizesCombo->currentData().value<BorderSize>()));
    if (auto listView = m_quickView->rootObject()->findChild<QQuickItem*>("listView")) {
        const int currentIndex = listView->property("currentIndex").toInt();
        if (currentIndex != -1) {
            const QModelIndex index = m_proxyModel->index(currentIndex, 0);
            if (index.isValid()) {
                config.writeEntry("library", index.data(Qt::UserRole + 4).toString());
                const QString theme = index.data(Qt::UserRole +5).toString();
                if (theme.isEmpty()) {
                    config.deleteEntry("theme");
                } else {
                    config.writeEntry("theme", theme);
                }
            }
        }
    }
    config.writeEntry("ButtonsOnLeft", buttonsToString(m_leftButtons->buttons()));
    config.writeEntry("ButtonsOnRight", buttonsToString(m_rightButtons->buttons()));
    config.sync();
    KCModule::save();
    // Send signal to all kwin instances
    QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/KWin"),
                                                      QStringLiteral("org.kde.KWin"),
                                                      QStringLiteral("reloadConfig"));
    QDBusConnection::sessionBus().send(message);
}

void ConfigurationModule::defaults()
{
    if (auto listView = m_quickView->rootObject()->findChild<QQuickItem*>("listView")) {
        const QModelIndex index = m_proxyModel->mapFromSource(m_model->findDecoration(s_defaultPlugin));
        listView->setProperty("currentIndex", index.isValid() ? index.row() : -1);
    }
    m_ui->borderSizesCombo->setCurrentIndex(m_ui->borderSizesCombo->findData(QVariant::fromValue(stringToSize(s_borderSizeNormal))));
    m_ui->closeWindowsDoubleClick->setChecked(false);
    KCModule::defaults();
}

void ConfigurationModule::showKNS(const QString &config)
{
    QPointer<KNS3::DownloadDialog> downloadDialog = new KNS3::DownloadDialog(config, this);
    if (downloadDialog->exec() == QDialog::Accepted && !downloadDialog->changedEntries().isEmpty()) {
        auto listView = m_quickView->rootObject()->findChild<QQuickItem*>("listView");
        QString selectedPluginName;
        QString selectedThemeName;
        if (listView) {
            const QModelIndex index = m_proxyModel->index(listView->property("currentIndex").toInt(), 0);
            if (index.isValid()) {
                selectedPluginName = index.data(Qt::UserRole + 4).toString();
                selectedThemeName = index.data(Qt::UserRole + 5).toString();
            }
        }
        m_model->init();
        if (!selectedPluginName.isEmpty()) {
            const QModelIndex index = m_model->findDecoration(selectedPluginName, selectedThemeName);
            const QModelIndex proxyIndex = m_proxyModel->mapFromSource(index);
            if (listView) {
                listView->setProperty("currentIndex", proxyIndex.isValid() ? proxyIndex.row() : -1);
            }
        }
    }
    delete downloadDialog;
}

bool ConfigurationModule::eventFilter(QObject *watched, QEvent *e)
{
    if (watched != m_ui) {
        return false;
    }
    if (e->type() == QEvent::PaletteChange) {
        updateColors();
    }
    return false;
}

void ConfigurationModule::updateColors()
{
    m_quickView->rootContext()->setContextProperty("backgroundColor", m_ui->palette().color(QPalette::Active, QPalette::Window));
    m_quickView->rootContext()->setContextProperty("highlightColor", m_ui->palette().color(QPalette::Active, QPalette::Shadow));
    m_quickView->rootContext()->setContextProperty("baseColor", m_ui->palette().color(QPalette::Active, QPalette::Base));
}

}
}

#include "kcm.moc"
