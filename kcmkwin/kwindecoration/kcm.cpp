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

// KDE
#include <KConfigGroup>
#include <KPluginFactory>
#include <KSharedConfig>
#include <KDecoration2/DecorationButton>
// Qt
#include <QDBusConnection>
#include <QDBusMessage>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
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
static const QString s_defaultPlugin = QStringLiteral("org.kde.breeze");

ConfigurationForm::ConfigurationForm(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
}

static bool s_loading = false;

ConfigurationModule::ConfigurationModule(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , m_model(new DecorationsModel(this))
    , m_ui(new ConfigurationForm(this))
{
    m_ui->view->rootContext()->setContextProperty(QStringLiteral("decorationsModel"), m_model);
    m_ui->view->rootContext()->setContextProperty("highlightColor", QPalette().color(QPalette::Highlight));
    m_ui->view->rootContext()->setContextProperty("_borderSizesIndex", 3); // 3 is normal
    m_ui->view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_ui->view->setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/kcm_kwindecoration/main.qml"))));
    if (m_ui->view->status() == QQuickWidget::Ready) {
        auto listView = m_ui->view->rootObject()->findChild<QQuickItem*>("listView");
        if (listView) {
            connect(listView, SIGNAL(currentIndexChanged()), this, SLOT(changed()));
        }
    }
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

    connect(m_ui->closeWindowsDoubleClick, &QCheckBox::stateChanged, this,
            static_cast<void (ConfigurationModule::*)()>(&ConfigurationModule::changed));
    connect(m_ui->closeWindowsDoubleClick, &QCheckBox::toggled, this,
        [this] (bool toggled) {
            if (!toggled || s_loading) {
                return;
            }
            m_ui->doubleClickMessage->animatedShow();
        }
    );
    connect(m_ui->borderSizesCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this] (int index) {
            auto listView = m_ui->view->rootObject()->findChild<QQuickItem*>("listView");
            if (listView) {
                listView->setProperty("borderSizesIndex", index);
            }
            changed();
        }
    );

    QVBoxLayout *l = new QVBoxLayout(this);
    l->addWidget(m_ui);
    QMetaObject::invokeMethod(m_model, "init", Qt::QueuedConnection);
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
    {QStringLiteral("Normal"), BorderSize::Normal},
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
    return s_sizes.key(size, QStringLiteral("Normal"));
}

void ConfigurationModule::load()
{
    s_loading = true;
    const KConfigGroup config = KSharedConfig::openConfig("kwinrc")->group(s_pluginName);
    const QString plugin = config.readEntry("library", s_defaultPlugin);
    const QString theme = config.readEntry("theme", QString());
    const QModelIndex index = m_model->findDecoration(plugin, theme);
    if (auto listView = m_ui->view->rootObject()->findChild<QQuickItem*>("listView")) {
        listView->setProperty("currentIndex", index.isValid() ? index.row() : -1);
    }
    m_ui->closeWindowsDoubleClick->setChecked(config.readEntry("CloseOnDoubleClickOnMenu", false));
    const QVariant border = QVariant::fromValue(stringToSize(config.readEntry("BorderSize", QStringLiteral("Normal"))));
    m_ui->borderSizesCombo->setCurrentIndex(m_ui->borderSizesCombo->findData(border));
    KCModule::load();
    s_loading = false;
}

void ConfigurationModule::save()
{
    KConfigGroup config = KSharedConfig::openConfig("kwinrc")->group(s_pluginName);
    config.writeEntry("CloseOnDoubleClickOnMenu", m_ui->closeWindowsDoubleClick->isChecked());
    config.writeEntry("BorderSize", sizeToString(m_ui->borderSizesCombo->currentData().value<BorderSize>()));
    if (auto listView = m_ui->view->rootObject()->findChild<QQuickItem*>("listView")) {
        const int currentIndex = listView->property("currentIndex").toInt();
        if (currentIndex != -1) {
            const QModelIndex index = m_model->index(currentIndex, 0);
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
    if (auto listView = m_ui->view->rootObject()->findChild<QQuickItem*>("listView")) {
        const QModelIndex index = m_model->findDecoration(s_defaultPlugin);
        listView->setProperty("currentIndex", index.isValid() ? index.row() : -1);
    }
    KCModule::defaults();
}

}
}

#include "kcm.moc"
