/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

// own
#include "tabboxview.h"
// tabbox
#include "clientitemdelegate.h"
#include "clientmodel.h"
#include "desktopitemdelegate.h"
#include "desktopmodel.h"
#include "tabboxconfig.h"
#include "tabboxhandler.h"

// Qt
#include <QTextStream>
#include <QGridLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QSizePolicy>
#include <QPainter>
#include <QPropertyAnimation>

// KDE
#include <kephal/screens.h>
#include <Plasma/FrameSvg>
#include <KDebug>
#include <KDE/Plasma/Theme>
#include <KDE/Plasma/WindowEffects>

namespace KWin
{
namespace TabBox
{

TabBoxView::TabBoxView(ClientModel *clientModel, DesktopModel *desktopModel, QWidget* parent)
    : QWidget(parent)
    , m_clientModel(clientModel)
    , m_desktopModel(desktopModel)
    , m_previewUpdate(false)
{
    setWindowFlags(Qt::X11BypassWindowManagerHint);
    setAttribute(Qt::WA_TranslucentBackground);
    QPalette pal = palette();
    pal.setColor(backgroundRole(), Qt::transparent);
    setPalette(pal);
    m_delegate = new ClientItemDelegate(this);
    m_additionalClientDelegate = new ClientItemDelegate(this);
    m_additionalClientDelegate->setShowSelection(false);
    m_desktopItemDelegate = new DesktopItemDelegate(this);
    m_additionalDesktopDelegate = new DesktopItemDelegate(this);
    m_tableView = new TabBoxMainView(this);
    m_additionalView = new TabBoxAdditionalView(this);

    // FrameSvg
    m_frame = new Plasma::FrameSvg(this);
    m_frame->setImagePath("dialogs/background");
    m_frame->setCacheAllRenderedFrames(true);
    m_frame->setEnabledBorders(Plasma::FrameSvg::AllBorders);

    m_selectionFrame = new Plasma::FrameSvg(this);
    m_selectionFrame->setImagePath("widgets/viewitem");
    m_selectionFrame->setElementPrefix("hover");
    m_selectionFrame->setCacheAllRenderedFrames(true);
    m_selectionFrame->setEnabledBorders(Plasma::FrameSvg::AllBorders);

    m_animation = new QPropertyAnimation(this, "selectedItem", this);
    m_animation->setDuration(250);
    m_animation->setEasingCurve(QEasingCurve::InOutQuad);

    connect(tabBox, SIGNAL(configChanged()), this, SLOT(configChanged()));
    connect(m_animation, SIGNAL(valueChanged(QVariant)), SLOT(update()));
    connect(m_tableView, SIGNAL(activated(QModelIndex)), SLOT(setCurrentIndex(QModelIndex)));
}

TabBoxView::~TabBoxView()
{
}

void TabBoxView::paintEvent(QPaintEvent* e)
{
    // paint the background
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setClipRect(e->rect());
    m_frame->paintFrame(&painter);
    // and the selection item
    if (m_previewUpdate) {
        m_previewUpdate = false;
        setCurrentIndex(m_tableView->currentIndex());
    }
    m_selectionFrame->paintFrame(&painter,
                                 m_tableView->geometry().topLeft() + m_selectedItem.topLeft());
    QWidget::paintEvent(e);
}

bool TabBoxView::event(QEvent* event)
{
    if (event->type() == QEvent::Paint) {
        QPainter p(this);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(rect(), Qt::transparent);
    }
    return QWidget::event(event);
}

void TabBoxView::resizeEvent(QResizeEvent* event)
{
    m_frame->resizeFrame(event->size());
    if (Plasma::Theme::defaultTheme()->windowTranslucencyEnabled()) {
        // blur background
        Plasma::WindowEffects::enableBlurBehind(winId(), true, m_frame->mask());
        Plasma::WindowEffects::overrideShadow(winId(), true);
    } else {
        // do not trim to mask with compositing enabled, otherwise shadows are cropped
        setMask(m_frame->mask());
    }
    QWidget::resizeEvent(event);
}

void TabBoxView::updateGeometry()
{
    if (m_tableView->model()->columnCount() == 0 || m_tableView->model()->rowCount() == 0)
        return;
    QSize hint = sizeHint();
    QRect screenRect = Kephal::ScreenUtils::screenGeometry(tabBox->activeScreen());
    int x = screenRect.x() + screenRect.width() * 0.5 - hint.width() * 0.5;
    int y = screenRect.y() + screenRect.height() * 0.5 - hint.height() * 0.5;

    setGeometry(x, y, hint.width(), hint.height());
}

QSize TabBoxView::sizeHint() const
{
    if (m_tableView->model()->columnCount() == 0 || m_tableView->model()->rowCount() == 0)
        return QSize(0, 0);
    // calculate width and height
    QSize tabBoxSize = m_tableView->sizeHint();
    qreal columnWidth = tabBoxSize.width() / m_tableView->model()->columnCount();
    qreal rowHeight = tabBoxSize.height() / m_tableView->model()->rowCount();
    for (int i = 0; i < m_tableView->model()->rowCount(); i++)
        m_tableView->setRowHeight(i, rowHeight);

    // additional view
    QSize additionalSize = m_additionalView->sizeHint();
    for (int i = 0; i < m_additionalView->model()->columnCount(); i++)
        m_additionalView->setColumnWidth(i, additionalSize.width());
    for (int i = 0; i < m_additionalView->model()->rowCount(); i++)
        m_additionalView->setRowHeight(i, additionalSize.height());

    // reserve some space for borders
    qreal top, bottom, left, right;
    m_frame->getMargins(left, top, right, bottom);
    int width = columnWidth * m_tableView->model()->columnCount() + left + right;
    int height = rowHeight * m_tableView->model()->rowCount() + top + bottom;
    qreal addedWidth = left + right;

    // depending on layout of additional view we have to add some width or height
    switch(tabBox->config().selectedItemViewPosition()) {
    case TabBoxConfig::AbovePosition: // fall through
    case TabBoxConfig::BelowPosition:
        if (additionalSize.width() + int(left + right) > width) {
            addedWidth += (additionalSize.width() + int(left + right) - width);
            width = additionalSize.width() + int(left + right);
        }
        height = height + additionalSize.height();
        break;
    case TabBoxConfig::LeftPosition: // fall through
    case TabBoxConfig::RightPosition:
        width = width + additionalSize.width();
        addedWidth += additionalSize.width();
        height = qMax(height, additionalSize.height() + int(top + bottom));
        break;
    default:
        // don't add
        break;
    }

    QRect screenRect = Kephal::ScreenUtils::screenGeometry(tabBox->activeScreen());
    width = qBound(screenRect.width() * tabBox->config().minWidth() / 100, width,
                   screenRect.width());
    height = qBound(screenRect.height() * tabBox->config().minHeight() / 100, height,
                    screenRect.height());
    if (width - addedWidth > tabBoxSize.width()) {
        columnWidth = (width - addedWidth) / m_tableView->model()->columnCount();
    }
    m_tableView->setMinimumWidth(width - addedWidth);
    for (int i = 0; i < m_tableView->model()->columnCount(); i++)
        m_tableView->setColumnWidth(i, columnWidth);
    return QSize(width, height);
}

void TabBoxView::setCurrentIndex(QModelIndex index)
{
    if (index.isValid()) {
        m_tableView->setCurrentIndex(index);
        const QRect visualRect = m_tableView->visualRect(index);
        if (m_selectedItem.isNull())
            m_selectedItem = visualRect;
        if (m_animation->state() == QPropertyAnimation::Running) {
            m_animation->stop();
            m_selectedItem = visualRect;
            update();
        } else {
            m_animation->setStartValue(m_selectedItem);
            m_animation->setEndValue(visualRect);
            m_animation->start();
        }
        m_selectionFrame->resizeFrame(visualRect.size());
        m_additionalView->setCurrentIndex(index);
    }
}

void TabBoxView::configChanged()
{
    switch(tabBox->config().tabBoxMode()) {
    case TabBoxConfig::ClientTabBox:
        m_tableView->setModel(m_clientModel);
        m_tableView->setItemDelegate(m_delegate);
        m_additionalView->setModel(m_clientModel);
        m_additionalView->setItemDelegate(m_additionalClientDelegate);
        break;
    case TabBoxConfig::DesktopTabBox:
        m_tableView->setModel(m_desktopModel);
        m_tableView->setItemDelegate(m_desktopItemDelegate);
        m_additionalView->setModel(m_desktopModel);
        m_additionalView->setItemDelegate(m_additionalDesktopDelegate);
        break;
    }
    QLayout* old = layout();
    QLayoutItem *child;
    while (old && (child = old->takeAt(0)) != 0) {
        delete child;
    }
    delete old;
    QBoxLayout *layout;
    switch(tabBox->config().selectedItemViewPosition()) {
    case TabBoxConfig::AbovePosition: {
        layout = new QVBoxLayout();
        QHBoxLayout* horizontalLayout1 = new QHBoxLayout();
        horizontalLayout1->addStretch();
        horizontalLayout1->addWidget(m_additionalView);
        horizontalLayout1->addStretch();
        layout->addLayout(horizontalLayout1);
        layout->addStretch();
        QHBoxLayout* horizontalLayout2 = new QHBoxLayout();
        horizontalLayout2->addStretch();
        horizontalLayout2->addWidget(m_tableView);
        horizontalLayout2->addStretch();
        layout->addLayout(horizontalLayout2);
        m_additionalView->show();
        break;
    }
    case TabBoxConfig::BelowPosition: {
        layout = new QVBoxLayout();
        QHBoxLayout* horizontalLayout1 = new QHBoxLayout();
        horizontalLayout1->addStretch();
        horizontalLayout1->addWidget(m_tableView);
        horizontalLayout1->addStretch();
        layout->addLayout(horizontalLayout1);
        layout->addStretch();
        QHBoxLayout* horizontalLayout2 = new QHBoxLayout();
        horizontalLayout2->addStretch();
        horizontalLayout2->addWidget(m_additionalView);
        horizontalLayout2->addStretch();
        layout->addLayout(horizontalLayout2);
        m_additionalView->show();
        break;
    }
    case TabBoxConfig::LeftPosition: {
        layout = new QHBoxLayout();
        QVBoxLayout* verticalLayout1 = new QVBoxLayout();
        verticalLayout1->addStretch();
        verticalLayout1->addWidget(m_additionalView);
        verticalLayout1->addStretch();
        layout->addLayout(verticalLayout1);
        layout->addStretch();
        QVBoxLayout* verticalLayout2 = new QVBoxLayout();
        verticalLayout2->addStretch();
        verticalLayout2->addWidget(m_tableView);
        verticalLayout2->addStretch();
        layout->addLayout(verticalLayout2);
        m_additionalView->show();
        break;
    }
    case TabBoxConfig::RightPosition: {
        layout = new QHBoxLayout();
        QVBoxLayout* verticalLayout1 = new QVBoxLayout();
        verticalLayout1->addStretch();
        verticalLayout1->addWidget(m_tableView);
        verticalLayout1->addStretch();
        layout->addLayout(verticalLayout1);
        layout->addStretch();
        QVBoxLayout* verticalLayout2 = new QVBoxLayout();
        verticalLayout2->addStretch();
        verticalLayout2->addWidget(m_additionalView);
        verticalLayout2->addStretch();
        layout->addLayout(verticalLayout2);
        m_additionalView->show();
        break;
    }
    default: {
        layout = new QVBoxLayout();
        layout->addStretch();
        layout->addWidget(m_tableView);
        layout->addStretch();
        m_additionalView->hide();
        break;
    }
    }
    setLayout(layout);
    if (m_preview)
        m_previewUpdate = true;
}

QModelIndex TabBoxView::indexAt(QPoint pos)
{
    return m_tableView->indexAt(m_tableView->mapFromParent(pos));
}

void TabBoxView::setPreview(bool preview)
{
    m_preview = preview;
    if (preview)
        m_frame->setImagePath("dialogs/opaque/background");
}

/********************************************************
* TabBoxMainView
********************************************************/

TabBoxMainView::TabBoxMainView(QWidget* parent)
    : QTableView(parent)
{
    setFrameStyle(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    viewport()->setAutoFillBackground(false);

    // adjust table view to needs of tabbox
    setShowGrid(false);
    horizontalHeader()->hide();
    verticalHeader()->hide();
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectItems);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerItem);
    setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

TabBoxMainView::~TabBoxMainView()
{
}

QSize TabBoxMainView::sizeHint() const
{
    int maxWidth = 0;
    int minWidth = sizeHintForColumn(0);
    int maxHeight = 0;
    int minHeight = sizeHintForRow(0);
    for (int i = 0; i < model()->columnCount(); i++) {
        minWidth = qMin(minWidth, sizeHintForColumn(i));
        maxWidth = qMax(maxWidth, sizeHintForColumn(i));
    }
    for (int i = 0; i < model()->rowCount(); i++) {
        minHeight = qMin(minHeight, sizeHintForRow(i));
        maxHeight = qMax(maxHeight, sizeHintForRow(i));
    }
    qreal columnWidth = (minWidth + qreal(maxWidth - minWidth) / 2.0);
    qreal rowHeight = (minHeight + qreal(maxHeight - minHeight) / 2.0);
    return QSize(columnWidth * model()->columnCount(),
                 rowHeight * model()->rowCount());
}

QModelIndex TabBoxMainView::moveCursor(QAbstractItemView::CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(cursorAction)
    Q_UNUSED(modifiers)
    return currentIndex();
}

/********************************************************
* TabBoxAdditonalView
********************************************************/

TabBoxAdditionalView::TabBoxAdditionalView(QWidget* parent)
    : QTableView(parent)
{
    setFrameStyle(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    viewport()->setAutoFillBackground(false);

    // adjust table view to needs of tabbox
    setShowGrid(false);
    horizontalHeader()->hide();
    verticalHeader()->hide();
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectItems);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerItem);
    setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

TabBoxAdditionalView::~TabBoxAdditionalView()
{
}

QSize TabBoxAdditionalView::sizeHint() const
{
    int maxWidth = 0;
    int minWidth = sizeHintForColumn(0);
    int maxHeight = 0;
    int minHeight = sizeHintForRow(0);
    for (int i = 0; i < model()->columnCount(); i++) {
        minWidth = qMin(minWidth, sizeHintForColumn(i));
        maxWidth = qMax(maxWidth, sizeHintForColumn(i));
    }
    for (int i = 0; i < model()->rowCount(); i++) {
        minHeight = qMin(minHeight, sizeHintForRow(i));
        maxHeight = qMax(maxHeight, sizeHintForRow(i));
    }
    qreal columnWidth = (minWidth + qreal(maxWidth - minWidth) / 2.0);
    qreal rowHeight = (minHeight + qreal(maxHeight - minHeight) / 2.0);
    return QSize(columnWidth, rowHeight);
}

QModelIndex TabBoxAdditionalView::moveCursor(QAbstractItemView::CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(cursorAction)
    Q_UNUSED(modifiers)
    return currentIndex();
}

void TabBoxAdditionalView::wheelEvent(QWheelEvent* event)
{
    Q_UNUSED(event)
}

} // namespace Tabbox
} // namespace KWin

#include "tabboxview.moc"
