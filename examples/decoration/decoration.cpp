/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "decoration.h"
#include "closebutton.h"

#include <KDecoration2/DecoratedClient>
#include <KDecoration2/DecorationSettings>

#include <QPainter>

BasicDecoration::BasicDecoration(QObject *parent, const QVariantList &args)
    : KDecoration2::Decoration(parent, args)
{
}

BasicDecoration::~BasicDecoration()
{
}

void BasicDecoration::setState(std::function<void(BasicDecorationState *state)> mutator)
{
    mutator(&m_pending);
    applyState(m_pending);
}

void BasicDecoration::applyState(const BasicDecorationState &state)
{
    QMargins borders(state.borderSize, state.borderSize, state.borderSize, state.borderSize);
    QMargins resizeOnlyBorders(state.resizeOnlyBorderSize, state.resizeOnlyBorderSize, state.resizeOnlyBorderSize, state.resizeOnlyBorderSize);
    switch (state.titlebarEdge) {
    case Qt::TopEdge:
        borders.setTop(state.titlebarSize);
        break;
    case Qt::RightEdge:
        borders.setRight(state.titlebarSize);
        break;
    case Qt::BottomEdge:
        borders.setBottom(state.titlebarSize);
        break;
    case Qt::LeftEdge:
        borders.setLeft(state.titlebarSize);
        break;
    }

    QRect titleBarRect;
    switch (state.titlebarEdge) {
    case Qt::TopEdge:
        titleBarRect = QRect(0, 0, state.clientSize.width(), state.titlebarSize);
        break;
    case Qt::RightEdge:
        titleBarRect = QRect(state.clientSize.width(), 0, state.titlebarSize, state.clientSize.height());
        break;
    case Qt::BottomEdge:
        titleBarRect = QRect(0, state.clientSize.height(), state.clientSize.width(), state.titlebarSize);
        break;
    case Qt::LeftEdge:
        titleBarRect = QRect(0, 0, state.titlebarSize, state.clientSize.height());
        break;
    }

    switch (state.titlebarEdge) {
    case Qt::TopEdge:
    case Qt::BottomEdge:
        m_frontButtonGroup->setOrientation(Qt::Horizontal);
        m_backButtonGroup->setOrientation(Qt::Horizontal);
        m_frontButtonGroup->setPos(QPointF(titleBarRect.x(), titleBarRect.y()));
        m_backButtonGroup->setPos(QPointF(titleBarRect.x() + titleBarRect.width() - m_backButtonGroup->geometry().width(), titleBarRect.y()));
        break;
    case Qt::LeftEdge:
    case Qt::RightEdge:
        m_frontButtonGroup->setOrientation(Qt::Vertical);
        m_backButtonGroup->setOrientation(Qt::Vertical);
        m_frontButtonGroup->setPos(QPointF(titleBarRect.x(), titleBarRect.y()));
        m_backButtonGroup->setPos(QPointF(titleBarRect.x(), titleBarRect.y() + titleBarRect.height() - m_backButtonGroup->geometry().height()));
        break;
    }
    m_frontButtonGroup->setSpacing(0);
    m_backButtonGroup->setSpacing(0);

    setBorders(borders);
    setResizeOnlyBorders(resizeOnlyBorders);
    setTitleBar(titleBarRect);

    m_current = m_pending;
    update();
}

static KDecoration2::DecorationButton *buttonCreator(KDecoration2::DecorationButtonType type, KDecoration2::Decoration *decoration, QObject *parent)
{
    switch (type) {
    case KDecoration2::DecorationButtonType::Close:
        return new BasicCloseButton(decoration, parent);
    default:
        return nullptr;
    }
}

int BasicDecoration::preferredTitlebarSize() const
{
    const QFontMetrics fontMetrics(settings()->font());
    const int baseUnit = settings()->gridUnit();
    return std::round(1.5 * baseUnit) + fontMetrics.height();
}

bool BasicDecoration::init()
{
    m_frontButtonGroup = new KDecoration2::DecorationButtonGroup(KDecoration2::DecorationButtonGroup::Position::Left, this, buttonCreator);
    m_backButtonGroup = new KDecoration2::DecorationButtonGroup(KDecoration2::DecorationButtonGroup::Position::Right, this, buttonCreator);

    connect(client(), &KDecoration2::DecoratedClient::sizeChanged, this, [this]() {
        setState([this](BasicDecorationState *state) {
            state->clientSize = client()->size();
        });
    });

    connect(client(), &KDecoration2::DecoratedClient::captionChanged, this, [this]() {
        update(titleBar());
    });

    setState([this](BasicDecorationState *state) {
        state->titlebarEdge = Qt::LeftEdge;
        state->titlebarSize = preferredTitlebarSize();
        state->borderSize = 0;
        state->resizeOnlyBorderSize = settings()->gridUnit(),
        state->clientSize = client()->size();
    });

    return true;
}

void BasicDecoration::paint(QPainter *painter, const QRect &repaintArea)
{
    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::blue);
    painter->drawRect(titleBar());
    painter->restore();

    {
        painter->save();

        QRect captionRect;
        switch (m_current.titlebarEdge) {
        case Qt::TopEdge:
            captionRect = QRect(0, 0, titleBar().width(), titleBar().height());
            painter->translate(titleBar().x(), titleBar().y());
            break;
        case Qt::RightEdge:
            captionRect = QRect(0, 0, titleBar().height(), titleBar().width());
            painter->translate(titleBar().x() + titleBar().width(), titleBar().y());
            painter->rotate(90);
            break;
        case Qt::BottomEdge:
            captionRect = QRect(0, 0, titleBar().width(), titleBar().height());
            painter->translate(titleBar().x(), titleBar().y());
            break;
        case Qt::LeftEdge:
            captionRect = QRect(0, 0, titleBar().height(), titleBar().width());
            painter->translate(titleBar().x(), titleBar().y() + titleBar().height());
            painter->rotate(-90);
            break;
        }

        painter->setFont(settings()->font());
        painter->setPen(Qt::white);
        painter->drawText(captionRect, Qt::AlignCenter, client()->caption());
        painter->restore();
    }

    m_frontButtonGroup->paint(painter, repaintArea);
    m_backButtonGroup->paint(painter, repaintArea);
}
