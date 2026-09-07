/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "showfpseffect.h"
#include "core/output.h"
#include "core/renderbackend.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "scene/workspacescene.h"

#include <QQmlContext>

namespace KWin
{

PaintDurationModel::PaintDurationModel(ssize_t count)
    : m_maxCount(count)
{
    m_values.resize(count);
}

int PaintDurationModel::rowCount(const QModelIndex &parent) const
{
    return m_values.size();
}

int PaintDurationModel::columnCount(const QModelIndex &parent) const
{
    return 1;
}

QVariant PaintDurationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= int(m_values.size()) || role != Qt::DisplayRole) {
        return {};
    }
    return m_values[index.row()];
}

QHash<int, QByteArray> PaintDurationModel::roleNames() const
{
    return {{Qt::DisplayRole, "value"}};
}

void PaintDurationModel::push(std::chrono::nanoseconds value)
{
    if (m_values.size() == m_maxCount) {
        beginRemoveRows({}, m_maxCount - 1, m_maxCount - 1);
        m_values.pop_back();
        endRemoveRows();
    }
    beginInsertRows({}, 0, 0);
    m_values.push_front(std::chrono::duration_cast<std::chrono::microseconds>(value).count());
    endInsertRows();
}

void PaintDurationModel::resize(ssize_t size)
{
    m_maxCount = size;
    beginResetModel();
    m_values.resize(size);
    endResetModel();
}

ShowFpsEffect::ShowFpsEffect()
{
    connect(effects->scene(), &WorkspaceScene::viewRemoved, this, &ShowFpsEffect::removeView);
}

ShowFpsEffect::~ShowFpsEffect()
{
}

void ShowFpsEffect::removeView(RenderView *view)
{
    m_data.erase(view);
}

ShowFpsScreen::ShowFpsScreen(uint32_t fps)
    : m_maximumFps(fps)
    , m_paintDuration(2 * fps)
    , m_paintDurationCPU(2 * fps)
{
}

int ShowFpsScreen::fps() const
{
    return m_fps;
}

int ShowFpsScreen::maximumFps() const
{
    return m_maximumFps;
}

PaintDurationModel *ShowFpsScreen::paintDuration()
{
    return &m_paintDuration;
}

PaintDurationModel *ShowFpsScreen::paintDurationCPU()
{
    return &m_paintDurationCPU;
}

QString ShowFpsScreen::presentationMode() const
{
    return m_presentationMode;
}

void ShowFpsScreen::presented(OutputFrame *frame, std::chrono::nanoseconds timestamp, PresentationMode mode)
{
    const auto cpu = frame->queryCpuRenderTime();
    const auto total = frame->queryRenderTime();
    if (cpu && total) {
        m_paintDuration.push(total->end - total->start);
        m_paintDurationCPU.push(cpu->end - cpu->start);
        Q_EMIT paintChanged();
    }
    QString presentMode;
    switch (mode) {
    case PresentationMode::VSync:
        presentMode = QStringLiteral("VSync");
        break;
    case PresentationMode::Async:
        presentMode = QStringLiteral("Tearing");
        break;
    case PresentationMode::AdaptiveSync:
        presentMode = QStringLiteral("Adaptive Sync");
        break;
    case PresentationMode::AdaptiveAsync:
        presentMode = QStringLiteral("Adaptive Sync + Tearing");
        break;
    }
    if (m_presentationMode != presentMode) {
        m_presentationMode = presentMode;
        Q_EMIT presentationModeChanged();
    }
}

void ShowFpsScreen::setMaximumFps(uint32_t fps)
{
    if (m_maximumFps == fps) {
        return;
    }
    m_maximumFps = fps;
    Q_EMIT maximumFpsChanged();

    m_paintDuration.resize(2 * fps);
    m_paintDurationCPU.resize(2 * fps);
}

class ShowFpsFeedback : public PresentationFeedback
{
public:
    explicit ShowFpsFeedback(ShowFpsScreen *screen)
        : m_screen(screen)
    {
    }

    void presented(OutputFrame *frame, std::chrono::nanoseconds timestamp,
                   PresentationMode mode, PresentationFeedbackFlags flags) override
    {
        if (m_screen) {
            m_screen->presented(frame, timestamp, mode);
        }
    }

    const QPointer<ShowFpsScreen> m_screen;
};

void ShowFpsEffect::prePaintScreen(ScreenPrePaintData &data)
{
    effects->prePaintScreen(data);
    if (!data.frame) {
        return;
    }

    uint32_t maximumFps = std::round(data.view->refreshRate() / 1000.0);

    auto &screenData = m_data[data.view];
    m_currentView = data.view;
    if (!screenData) {
        screenData = std::make_unique<ShowFpsScreen>(maximumFps);
    }
    data.frame->addFeedback(std::make_shared<ShowFpsFeedback>(screenData.get()), PresentationFeedbackFlags{});

    screenData->m_newFps += 1;
    screenData->setMaximumFps(maximumFps);

    if (!screenData->m_scene) {
        screenData->m_scene = std::make_unique<OffscreenQuickScene>();
        screenData->m_scene->setAutomaticRepaint(false);
        screenData->m_scene->setAutomaticFrame(false);
        screenData->m_scene->loadFromModule(QStringLiteral("org.kde.kwin.showfps"), QStringLiteral("Main"), {{QStringLiteral("effect"), QVariant::fromValue(screenData.get())}});
        if (!screenData->m_scene->rootItem()) {
            // main-fallback.qml has less dependencies than main.qml, so it should work on any system where kwin compiles
            screenData->m_scene->loadFromModule(QStringLiteral("org.kde.kwin.showfps"), QStringLiteral("Fallback"), {{QStringLiteral("effect"), QVariant::fromValue(screenData.get())}});
        }
    }
    auto now = std::chrono::steady_clock::now();
    if ((now - screenData->m_lastFpsTime) >= std::chrono::milliseconds(1000)) {
        screenData->m_fps = screenData->m_newFps;
        screenData->m_newFps = 0;
        screenData->m_lastFpsTime = now;
        Q_EMIT screenData->fpsChanged();
    }

    const auto rect = data.view->viewport();
    screenData->m_scene->setGeometry(QRect(rect.x() + rect.width() - 300, rect.y(), 300, 150));
    screenData->m_scene->update(data.frame);
}

bool ShowFpsEffect::blocksDirectScanout() const
{
    return false;
}

bool ShowFpsEffect::supported()
{
    return effects->isOpenGLCompositing();
}

} // namespace KWin

#include "moc_showfpseffect.cpp"
