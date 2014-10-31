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
// Qt
#include <QDBusConnection>
#include <QDBusMessage>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QStandardPaths>
#include <QVBoxLayout>

K_PLUGIN_FACTORY(KDecorationFactory,
                 registerPlugin<KDecoration2::Configuration::ConfigurationModule>();
                )

namespace KDecoration2
{

namespace Configuration
{
static const QString s_pluginName = QStringLiteral("org.kde.kdecoration2");
static const QString s_defaultPlugin = QStringLiteral("org.kde.breeze");

ConfigurationModule::ConfigurationModule(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
    , m_view(new QQuickWidget(this))
    , m_model(new DecorationsModel(this))
{
    m_view->rootContext()->setContextProperty(QStringLiteral("decorationsModel"), m_model);
    m_view->rootContext()->setContextProperty("highlightColor", QPalette().color(QPalette::Highlight));
    m_view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_view->setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/kcm_kwindecoration/main.qml"))));
    if (m_view->status() == QQuickWidget::Ready) {
        auto listView = m_view->rootObject()->findChild<QQuickItem*>("listView");
        if (listView) {
            connect(listView, SIGNAL(currentIndexChanged()), this, SLOT(changed()));
        }
    }

    QVBoxLayout *l = new QVBoxLayout(this);
    l->addWidget(m_view);
    QMetaObject::invokeMethod(m_model, "init", Qt::QueuedConnection);
}

ConfigurationModule::~ConfigurationModule() = default;

void ConfigurationModule::showEvent(QShowEvent *ev)
{
    KCModule::showEvent(ev);
}

void ConfigurationModule::load()
{
    const KConfigGroup config = KSharedConfig::openConfig("kwinrc")->group(s_pluginName);
    const QString plugin = config.readEntry("library", s_defaultPlugin);
    const QString theme = config.readEntry("theme", QString());
    const QModelIndex index = m_model->findDecoration(plugin, theme);
    if (auto listView = m_view->rootObject()->findChild<QQuickItem*>("listView")) {
        listView->setProperty("currentIndex", index.isValid() ? index.row() : -1);
    }
    KCModule::load();
}

void ConfigurationModule::save()
{
    if (auto listView = m_view->rootObject()->findChild<QQuickItem*>("listView")) {
        const int currentIndex = listView->property("currentIndex").toInt();
        if (currentIndex != -1) {
            const QModelIndex index = m_model->index(currentIndex, 0);
            if (index.isValid()) {
                KConfigGroup config = KSharedConfig::openConfig("kwinrc")->group(s_pluginName);
                config.writeEntry("library", index.data(Qt::UserRole + 4).toString());
                const QString theme = index.data(Qt::UserRole +5).toString();
                if (theme.isEmpty()) {
                    config.deleteEntry("theme");
                } else {
                    config.writeEntry("theme", theme);
                }
                config.sync();
            }
        }
    }
    KCModule::save();
    // Send signal to all kwin instances
    QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/KWin"),
                                                      QStringLiteral("org.kde.KWin"),
                                                      QStringLiteral("reloadConfig"));
    QDBusConnection::sessionBus().send(message);
}

void ConfigurationModule::defaults()
{
    if (auto listView = m_view->rootObject()->findChild<QQuickItem*>("listView")) {
        const QModelIndex index = m_model->findDecoration(s_defaultPlugin);
        listView->setProperty("currentIndex", index.isValid() ? index.row() : -1);
    }
    KCModule::defaults();
}

}
}

#include "kcm.moc"
