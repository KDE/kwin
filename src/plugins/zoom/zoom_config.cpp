/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010 Sebastian Sauer <sebsauer@kdab.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "zoom_config.h"

#include "config-kwin.h"

// KConfigSkeleton
#include "zoomconfig.h"
#include <kwineffects_interface.h>

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KWindowSystem>

#include <QAction>
#include <QFontDatabase>
#include <QVBoxLayout>

K_PLUGIN_CLASS(KWin::ZoomEffectConfig)

namespace KWin
{

static QString encodeModifierOnlySequence(const QKeySequence &sequence)
{
    if (sequence.isEmpty()) {
        return QString();
    }

    // Normalize from modifiers+key to modifiers
    Qt::KeyboardModifiers modifiers = sequence[0].keyboardModifiers();
    switch (sequence[0].key()) {
    case Qt::Key_Meta:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
        modifiers |= Qt::MetaModifier;
        break;
    case Qt::Key_Shift:
        modifiers |= Qt::ShiftModifier;
        break;
    case Qt::Key_Control:
        modifiers |= Qt::ControlModifier;
        break;
    case Qt::Key_Alt:
        modifiers |= Qt::AltModifier;
        break;
    default:
        break;
    }

    QStringList encoded;
    if (modifiers & Qt::MetaModifier) {
        encoded += QLatin1String("Meta");
    }
    if (modifiers & Qt::ControlModifier) {
        encoded += QLatin1String("Ctrl");
    }
    if (modifiers & Qt::AltModifier) {
        encoded += QLatin1String("Alt");
    }
    if (modifiers & Qt::ShiftModifier) {
        encoded += QLatin1String("Shift");
    }

    return encoded.join(QLatin1Char('+'));
}

static QKeySequence decodeModifierOnlySequence(const QString &string)
{
    const QStringList parts = string.split(QLatin1Char('+'));
    if (parts.isEmpty()) {
        return QKeySequence();
    }

    Qt::KeyboardModifiers modifiers;
    for (const QString &part : parts) {
        if (part == QLatin1String("Meta")) {
            modifiers |= Qt::MetaModifier;
        } else if (part == QLatin1String("Ctrl")) {
            modifiers |= Qt::ControlModifier;
        } else if (part == QLatin1String("Alt")) {
            modifiers |= Qt::AltModifier;
        } else if (part == QLatin1String("Shift")) {
            modifiers |= Qt::ShiftModifier;
        }
    }

    // Normalize the key sequence to modifiers+key. QKeySequence doesn't understand sequences that contain only modifiers.
    if (modifiers & Qt::ShiftModifier) {
        return Qt::Key_Shift | (modifiers & ~Qt::ShiftModifier);
    } else if (modifiers & Qt::AltModifier) {
        return Qt::Key_Alt | (modifiers & ~Qt::AltModifier);
    } else if (modifiers & Qt::ControlModifier) {
        return Qt::Key_Control | (modifiers & ~Qt::ControlModifier);
    } else if (modifiers & Qt::MetaModifier) {
        return Qt::Key_Meta | (modifiers & ~Qt::MetaModifier);
    } else {
        return QKeySequence();
    }
}

PointerAxisGestureModifiersWidget::PointerAxisGestureModifiersWidget(QWidget *parent)
    : KKeySequenceWidget(parent)
{
    setProperty("kcfg_property", QStringLiteral("modifiers"));
    setCheckForConflictsAgainst(None);
    setPatterns(KKeySequenceRecorder::Pattern::Modifier);
    connect(this, &KKeySequenceWidget::keySequenceChanged, this, &PointerAxisGestureModifiersWidget::modifiersChanged);
}

QString PointerAxisGestureModifiersWidget::modifiers() const
{
    return encodeModifierOnlySequence(keySequence());
}

void PointerAxisGestureModifiersWidget::setModifiers(const QString &modifiers)
{
    setKeySequence(decodeModifierOnlySequence(modifiers));
}

ZoomEffectConfig::ZoomEffectConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    ZoomConfig::instance(KWIN_CONFIG);
    m_ui.setupUi(widget());

    connect(m_ui.editor, &KShortcutsEditor::keyChange, this, &KCModule::markAsChanged);

#if !HAVE_ACCESSIBILITY
    m_ui.kcfg_EnableFocusTracking->setVisible(false);
    m_ui.kcfg_EnableTextCaretTracking->setVisible(false);
#endif

    // Shortcut config. The shortcut belongs to the component "kwin"!
    KActionCollection *actionCollection = new KActionCollection(this, QStringLiteral("kwin"));
    actionCollection->setComponentDisplayName(i18n("KWin"));
    actionCollection->setConfigGroup(QStringLiteral("Zoom"));
    actionCollection->setConfigGlobal(true);

    QAction *a;
    a = actionCollection->addAction(KStandardActions::ZoomIn);
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Plus) << (Qt::META | Qt::Key_Equal));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Plus) << (Qt::META | Qt::Key_Equal));

    a = actionCollection->addAction(KStandardActions::ZoomOut);
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Minus));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_Minus));

    a = actionCollection->addAction(KStandardActions::ActualSize);
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_0));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_0));

    a = actionCollection->addAction(QStringLiteral("MoveZoomLeft"));
    a->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
    a->setText(i18n("Move Left"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_Left));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_Left));

    a = actionCollection->addAction(QStringLiteral("MoveZoomRight"));
    a->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
    a->setText(i18n("Move Right"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_Right));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_Right));

    a = actionCollection->addAction(QStringLiteral("MoveZoomUp"));
    a->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    a->setText(i18n("Move Up"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_Up));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_Up));

    a = actionCollection->addAction(QStringLiteral("MoveZoomDown"));
    a->setIcon(QIcon::fromTheme(QStringLiteral("go-down")));
    a->setText(i18n("Move Down"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_Down));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::CTRL | Qt::Key_Down));

    a = actionCollection->addAction(QStringLiteral("MoveMouseToFocus"));
    a->setIcon(QIcon::fromTheme(QStringLiteral("view-restore")));
    a->setText(i18n("Move Mouse to Focus"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_F5));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_F5));

    a = actionCollection->addAction(QStringLiteral("MoveMouseToCenter"));
    a->setIcon(QIcon::fromTheme(QStringLiteral("view-restore")));
    a->setText(i18n("Move Mouse to Center"));
    a->setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_F6));
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << (Qt::META | Qt::Key_F6));

    if (KWindowSystem::isPlatformWayland()) {
        auto axisGesture = new PointerAxisGestureModifiersWidget();
        axisGesture->setObjectName(QLatin1String("kcfg_PointerAxisGestureModifiers"));
        m_ui.formLayout->addRow(new QLabel(i18n("Scroll gesture modifier keys:")), axisGesture);

        auto description = new QLabel(i18n("Scroll while the modifier keys are pressed to zoom"));
        description->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
        m_ui.formLayout->addRow(nullptr, description);
    }

    m_ui.editor->addCollection(actionCollection);

    addConfig(ZoomConfig::self(), widget());
}

void ZoomEffectConfig::save()
{
    m_ui.editor->save(); // undo() will restore to this state from now on
    KCModule::save();
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("zoom"));
}

} // namespace

#include "zoom_config.moc"

#include "moc_zoom_config.cpp"
