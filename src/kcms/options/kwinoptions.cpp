/*
    SPDX-FileCopyrightText: 2026 Tobias Ozór <tobiasozor@outlook.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwinoptions.h"
#include "kwinoptionsdata.h"

#include <KGlobalAccel>
#include <KPluginFactory>
#include <QAction>
#include <QDBusConnection>
#include <QDBusMessage>
#include <qqml.h>

K_PLUGIN_FACTORY_WITH_JSON(KWinOptionsFactory, "kcm_kwinoptions.json", registerPlugin<KWinOptions>(); registerPlugin<KWinOptionsData>();)

KWinOptions::KWinOptions(QObject *parent, const KPluginMetaData &metaData)
    : KQuickManagedConfigModule(parent, metaData)
    , m_kwinSettings(new KWinOptionsSettings(this))
    , m_untileActiveScreenAction(new QAction(this))
{
    qmlRegisterAnonymousType<KWinOptionsSettings>("org.kde.plasma.kcm.kwinoptions", 0);

    qmlRegisterUncreatableType<KWinOptionsSettings>("org.kde.plasma.kcm.kwinoptions",
                                                    1,
                                                    0,
                                                    "KWinOptionsSettings",
                                                    QStringLiteral("Registered for enum access only"));

    m_untileActiveScreenAction->setProperty("componentName", "kwin");
    m_untileActiveScreenAction->setObjectName("Untile Active Screen");
    m_untileActiveScreenAction->setProperty("isConfigurationAction", true);
    connect(this, &KWinOptions::untileActiveScreenShortcutChanged, this, &KWinOptions::settingsChanged);

    setButtons(Apply | Default | Help);
}

void KWinOptions::load()
{
    KQuickManagedConfigModule::load();

    const QKeySequence shortcut = KGlobalAccel::self()->globalShortcut(m_untileActiveScreenAction->property("componentName").toString(), m_untileActiveScreenAction->objectName()).value(0);
    m_untileActiveScreenShortcutInitialValue = shortcut;

    if (m_untileActiveScreenShortcut != shortcut) {
        m_untileActiveScreenShortcut = shortcut;
        Q_EMIT untileActiveScreenShortcutChanged();
    }
}

void KWinOptions::save()
{
    KQuickManagedConfigModule::save();

    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);

    if (m_untileActiveScreenShortcut != m_untileActiveScreenShortcutInitialValue) {
        KGlobalAccel::self()->setShortcut(m_untileActiveScreenAction, {m_untileActiveScreenShortcut}, KGlobalAccel::NoAutoloading);
        m_untileActiveScreenShortcutInitialValue = m_untileActiveScreenShortcut;
    }
}

bool KWinOptions::isSaveNeeded() const
{
    return m_untileActiveScreenShortcut != m_untileActiveScreenShortcutInitialValue;
}

bool KWinOptions::isPiPEnabled() const
{
    return qEnvironmentVariableIntegerValue("KWIN_WAYLAND_SUPPORT_XX_PIP_V1").value_or(1);
}

KWinOptionsSettings *KWinOptions::kwinSettings() const
{
    return m_kwinSettings;
}

QKeySequence KWinOptions::untileActiveScreenShortcut() const
{
    return m_untileActiveScreenShortcut;
}

void KWinOptions::setUntileActiveScreenShortcut(const QKeySequence &sequence)
{
    if (m_untileActiveScreenShortcut != sequence) {
        m_untileActiveScreenShortcut = sequence;
        Q_EMIT untileActiveScreenShortcutChanged();
    }
}

#include "kwinoptions.moc"
#include "moc_kwinoptions.cpp"
