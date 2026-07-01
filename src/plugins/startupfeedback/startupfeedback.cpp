/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "startupfeedback.h"
// Qt
#include <QApplication>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QFile>
#include <QPainter>
#include <QSize>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
// KDE
#include <KConfigGroup>
#include <KSharedConfig>
#include <KWindowSystem>
// KWin
#include "core/output.h"
#include "core/pixelgrid.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "cursor.h"
#include "cursorsource.h"
#include "effect/effecthandler.h"
#include "scene/imageitem.h"
#include "scene/workspacescene.h"

// based on StartupId in KRunner by Lubos Lunak
// SPDX-FileCopyrightText: 2001 Lubos Lunak <l.lunak@kde.org>

Q_LOGGING_CATEGORY(KWIN_STARTUPFEEDBACK, "kwin_effect_startupfeedback", QtWarningMsg)

namespace KWin
{

// duration of one bounce animation
static const int BOUNCE_DURATION = 500;
// number of key frames for blinking animation
static const int BLINKING_FRAMES = 5;
// duration between two key frames in msec
static const int BLINKING_FRAME_DURATION = 100;
// duration of one blinking animation
static const int BLINKING_DURATION = BLINKING_FRAME_DURATION * BLINKING_FRAMES;
// const int color_to_pixmap[] = { 0, 1, 2, 3, 2, 1 };
static const int FRAME_TO_BLINKING_COLOR[] = {
    0, 1, 2, 3, 2, 1};
static const QColor BLINKING_COLORS[] = {
    Qt::black, Qt::darkGray, Qt::lightGray, Qt::white, Qt::white};
static const int s_startupDefaultTimeout = 5;

StartupFeedbackEffect::StartupFeedbackEffect()
    : m_bounceSizesRatio(1.0)
#if KWIN_BUILD_X11
    , m_startupInfo(new KStartupInfo(KStartupInfo::CleanOnCantDetect, this))
#endif
    , m_active(false)
    , m_frame(0)
    , m_progress(0)
    , m_type(BouncingFeedback)
    , m_cursorSize(24)
    , m_configWatcher(KConfigWatcher::create(KSharedConfig::openConfig("klaunchrc", KConfig::NoGlobals)))
    , m_splashVisible(false)
{
#if KWIN_BUILD_X11
    connect(m_startupInfo, &KStartupInfo::gotNewStartup, this, [](const KStartupInfoId &id, const KStartupInfoData &data) {
        const auto icon = QIcon::fromTheme(data.findIcon(), QIcon::fromTheme(QStringLiteral("system-run")));
        Q_EMIT effects->startupAdded(id.id(), icon);
    });
    connect(m_startupInfo, &KStartupInfo::gotRemoveStartup, this, [](const KStartupInfoId &id, const KStartupInfoData &data) {
        Q_EMIT effects->startupRemoved(id.id());
    });
    connect(m_startupInfo, &KStartupInfo::gotStartupChange, this, [](const KStartupInfoId &id, const KStartupInfoData &data) {
        const auto icon = QIcon::fromTheme(data.findIcon(), QIcon::fromTheme(QStringLiteral("system-run")));
        Q_EMIT effects->startupChanged(id.id(), icon);
    });
#endif

    connect(effects, &EffectsHandler::startupAdded, this, &StartupFeedbackEffect::gotNewStartup);
    connect(effects, &EffectsHandler::startupRemoved, this, &StartupFeedbackEffect::gotRemoveStartup);
    connect(effects, &EffectsHandler::startupChanged, this, &StartupFeedbackEffect::gotStartupChange);

    connect(m_configWatcher.data(), &KConfigWatcher::configChanged, this, [this]() {
        reconfigure(ReconfigureAll);
    });
    reconfigure(ReconfigureAll);

    m_splashVisible = QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.KSplash"));
    auto serviceWatcher = new QDBusServiceWatcher(QStringLiteral("org.kde.KSplash"), QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(serviceWatcher, &QDBusServiceWatcher::serviceRegistered, this, [this] {
        m_splashVisible = true;
        stop();
    });
    connect(serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] {
        m_splashVisible = false;
        gotRemoveStartup({}); // Start the next feedback
    });
}

StartupFeedbackEffect::~StartupFeedbackEffect()
{
}

bool StartupFeedbackEffect::supported()
{
    return effects->animationsSupported();
}

void StartupFeedbackEffect::reconfigure(Effect::ReconfigureFlags flags)
{
    KConfigGroup c = m_configWatcher->config()->group(QStringLiteral("FeedbackStyle"));
    const bool busyCursor = c.readEntry("BusyCursor", true);

    c = m_configWatcher->config()->group(QStringLiteral("BusyCursorSettings"));
    m_timeout = std::chrono::seconds(c.readEntry("Timeout", s_startupDefaultTimeout));
#if KWIN_BUILD_X11
    m_startupInfo->setTimeout(m_timeout.count());
#endif
    const bool busyBlinking = c.readEntry("Blinking", false);
    const bool busyBouncing = c.readEntry("Bouncing", true);
    if (!busyCursor) {
        m_type = NoFeedback;
    } else if (busyBouncing) {
        m_type = BouncingFeedback;
    } else if (busyBlinking) {
        m_type = BlinkingFeedback;
    } else {
        m_type = PassiveFeedback;
    }
    if (m_active) {
        stop();
        start(m_startups[m_currentStartup]);
    }
}

void StartupFeedbackEffect::prePaintScreen(ScreenPrePaintData &data)
{
    const int time = m_clock.tick(data.view).count();

    if (m_active && effects->isCursorHidden()) {
        stop();
    }
    if (m_active) {
        switch (m_type) {
        case BouncingFeedback: {
            m_progress = (m_progress + time) % BOUNCE_DURATION;
            const qreal progressRatio = qreal(m_progress) / BOUNCE_DURATION;
            const qreal squeeze = std::pow(std::cos((progressRatio - 0.25) * M_PI), 2) * 24 - 12;
            const QSize bounceSize = QSize(64 + squeeze, 64 - squeeze);
            QTransform transform;
            transform.scale(bounceSize.width() / 64.0, bounceSize.height() / 64.0);
            transform.translate(-bounceSize.width() / 16.0, -bounceSize.height() / 16.0);
            m_item->setTransform(transform);
        } break;
        case BlinkingFeedback: {
            m_progress = (m_progress + time) % BLINKING_DURATION;
            m_frame = qRound((qreal)m_progress / (qreal)BLINKING_FRAME_DURATION) % BLINKING_FRAMES;
            QImage img(1, 1, QImage::Format_ARGB32_Premultiplied);
            img.fill(BLINKING_COLORS[FRAME_TO_BLINKING_COLOR[m_frame]]);
            m_blinkItem->setImage(img);
        } break;
        default:
            break; // nothing
        }
        m_item->setPosition(feedbackOffset());
    }
    effects->prePaintScreen(data);
}

void StartupFeedbackEffect::postPaintScreen()
{
    if (m_active) {
        m_item->scheduleFrame();
    }
    effects->postPaintScreen();
}

void StartupFeedbackEffect::gotNewStartup(const QString &id, const QIcon &icon)
{
    if (Cursors::self()->isCursorHidden()) {
        return;
    }

    const Cursor *mouse = Cursors::self()->mouse();
    if (mouse->source() && mouse->source()->isBlank()) {
        return;
    }

    Startup &startup = m_startups[id];
    startup.icon = icon;

    startup.expiredTimer = std::make_unique<QTimer>();
    // Stop the animation if the startup doesn't finish within reasonable interval.
    connect(startup.expiredTimer.get(), &QTimer::timeout, this, [this, id]() {
        gotRemoveStartup(id);
    });
    startup.expiredTimer->setSingleShot(true);
    startup.expiredTimer->start(m_timeout);

    m_currentStartup = id;
    start(startup);
}

void StartupFeedbackEffect::gotRemoveStartup(const QString &id)
{
    if (!m_startups.remove(id)) {
        return;
    }
    if (m_startups.isEmpty()) {
        m_currentStartup.clear();
        stop();
        return;
    }
    m_currentStartup = m_startups.begin().key();
    start(m_startups[m_currentStartup]);
}

void StartupFeedbackEffect::gotStartupChange(const QString &id, const QIcon &icon)
{
    if (m_currentStartup == id) {
        Startup &currentStartup = m_startups[m_currentStartup];
        if (!icon.isNull() && icon.name() != currentStartup.icon.name()) {
            currentStartup.icon = icon;
            start(currentStartup);
        }
    }
}

void StartupFeedbackEffect::start(const Startup &startup)
{
    if (m_type == NoFeedback || m_splashVisible || effects->isCursorHidden()) {
        return;
    }

    const LogicalOutput *output = effects->screenAt(effects->cursorPos().toPoint());
    if (!output) {
        return;
    }
    m_active = true;

    // read details about the mouse-cursor theme define per default
    KConfigGroup mousecfg(effects->inputConfig(), QStringLiteral("Mouse"));
    m_cursorSize = mousecfg.readEntry("cursorSize", 24);

    int iconSize = m_cursorSize / 1.5;
    if (!iconSize) {
        iconSize = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
    }
    // get ratio for bouncing cursor so we don't need to manually calculate the sizes for each icon size
    if (m_type == BouncingFeedback) {
        m_bounceSizesRatio = iconSize / 16.0;
    }

    m_item = std::make_unique<ImageItem>(effects->scene()->cursorItem());
    m_item->setPosition(feedbackOffset());
    m_item->setImage(startup.icon.pixmap(QSize(iconSize, iconSize), output->scale()).toImage());
    if (m_type == BouncingFeedback) {
        m_item->setSize(QSize(iconSize, iconSize) * m_bounceSizesRatio);
    } else {
        m_item->setSize(QSize(iconSize, iconSize));
    }
    if (m_type == BlinkingFeedback) {
        m_blinkItem = std::make_unique<ImageItem>(m_item.get());
        QImage img(1, 1, QImage::Format_ARGB32_Premultiplied);
        img.fill(BLINKING_COLORS[0]);
        m_blinkItem->setImage(img);
        m_blinkItem->setSize(QSize(iconSize, iconSize));
        m_blinkItem->setZ(-1);
    }
}

void StartupFeedbackEffect::stop()
{
    if (!m_active) {
        return;
    }
    m_active = false;
    m_clock.reset();
    m_item.reset();
}

QSize StartupFeedbackEffect::feedbackIconSize() const
{
    return QSize(20, 20) * m_bounceSizesRatio;
}

QPoint StartupFeedbackEffect::feedbackOffset() const
{
    int xDiff;
    if (m_cursorSize <= 16) {
        xDiff = 8 + 7;
    } else if (m_cursorSize <= 32) {
        xDiff = 16 + 7;
    } else if (m_cursorSize <= 48) {
        xDiff = 24 + 7;
    } else {
        xDiff = 32 + 7;
    }
    int yDiff = xDiff;
    int yOffset = 0;
    switch (m_type) {
    case BouncingFeedback: {
        const qreal progressRatio = qreal(m_progress) / BOUNCE_DURATION;
        yOffset = std::sin((progressRatio + 1) * M_PI) * 24 + 8;
    } break;
    case BlinkingFeedback: // fall through
    case PassiveFeedback:
        break;
    default:
        // nothing
        break;
    }
    return QPoint(xDiff, yDiff + yOffset);
}

bool StartupFeedbackEffect::isActive() const
{
    return m_active;
}

} // namespace

#include "moc_startupfeedback.cpp"
