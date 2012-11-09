/*
    This is the new kwindecoration kcontrol module

    Copyright (c) 2009, Urs Wolfer <uwolfer @ kde.org>
    Copyright (c) 2004, Sandro Giessl <sandro@giessl.com>
    Copyright (c) 2001
        Karol Szwed <gallium@kde.org>
        http://gallium.n3.net/

    Supports new kwin configuration plugins, and titlebar button position
    modification via dnd interface.

    Based on original "kwintheme" (Window Borders)
    Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/

#include "buttons.h"
#include "pixmaps.h"
#include "config-kwin.h"

#include <QApplication>
#include <QPainter>
#include <QLabel>
#include <QLayout>
#include <QMimeData>
#include <QScrollBar>

#include <klocale.h>
#include <kglobalsettings.h>

#ifdef KWIN_BUILD_KAPPMENU
#include <KConfigGroup>
#include <KConfig>
#endif

#include <kdecorationfactory.h>


#define BUTTONDRAGMIMETYPE "application/x-kde_kwindecoration_buttons"

namespace KWin
{

ButtonDrag::ButtonDrag(Button btn)
    : QMimeData()
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << btn.name;
    stream << btn.icon;
    stream << btn.type.unicode();
    stream << (int) btn.duplicate;
    stream << (int) btn.supported;
    setData(BUTTONDRAGMIMETYPE, data);
}


bool ButtonDrag::canDecode(QDropEvent* e)
{
    return e->mimeData()->hasFormat(BUTTONDRAGMIMETYPE);
}

bool ButtonDrag::decode(QDropEvent* e, Button& btn)
{
    QByteArray data = e->mimeData()->data(BUTTONDRAGMIMETYPE);
    if (data.size()) {
        e->accept();
        QDataStream stream(data);
        stream >> btn.name;
        stream >> btn.icon;
        ushort type;
        stream >> type;
        btn.type = QChar(type);
        int duplicate;
        stream >> duplicate;
        btn.duplicate = duplicate;
        int supported;
        stream >> supported;
        btn.supported = supported;
        return true;
    }
    return false;
}


Button::Button()
{
}

Button::Button(const QString& n, const QBitmap& i, QChar t, bool d, bool s)
    : name(n),
      icon(i),
      type(t),
      duplicate(d),
      supported(s)
{
}

Button::~Button()
{
}

// helper function to deal with the Button's bitmaps more easily...
QPixmap bitmapPixmap(const QBitmap& bm, const QColor& color)
{
    QPixmap pm(bm.size());
    pm.fill(Qt::white);
    QPainter p(&pm);
    p.setPen(color);
    p.drawPixmap(0, 0, bm);
    p.end();
    pm.setMask(pm.createMaskFromColor(Qt::white));
    return pm;
}


ButtonSource::ButtonSource(QWidget *parent)
    : QListWidget(parent)
{
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(false);
    setSortingEnabled(true);
}

ButtonSource::~ButtonSource()
{
}

QSize ButtonSource::sizeHint() const
{
    // make the sizeHint height a bit smaller than the one of QListView...

    ensurePolished();

    QSize s;
    if (verticalScrollBar()->isVisible())
        s.setWidth(s.width() + style()->pixelMetric(QStyle::PM_ScrollBarExtent));
    s += QSize(frameWidth() * 2, frameWidth() * 2);

    // size hint: 4 lines of text...
    s.setHeight(s.height() + fontMetrics().lineSpacing() * 3);

    return s;
}

void ButtonSource::hideAllButtons()
{
    for (int i = 0; i < count(); i++) {
        item(i)->setHidden(true);
    }
}

void ButtonSource::showAllButtons()
{
    for (int i = 0; i < count(); i++) {
        item(i)->setHidden(false);
    }
}

void ButtonSource::showButton(QChar btn)
{
    for (int i = 0; i < count(); i++) {
        ButtonSourceItem *buttonItem = dynamic_cast<ButtonSourceItem*>(item(i));
        if (buttonItem && buttonItem->button().type == btn) {
            item(i)->setHidden(false);
            return;
        }
    }
}

void ButtonSource::hideButton(QChar btn)
{
    for (int i = 0; i < count(); i++) {
        ButtonSourceItem *buttonItem = dynamic_cast<ButtonSourceItem*>(item(i));
        if (buttonItem && buttonItem->button().type == btn && !buttonItem->button().duplicate) {
            item(i)->setHidden(true);
            return;
        }
    }
}

void ButtonSource::dragMoveEvent(QDragMoveEvent *e)
{
    e->setAccepted(ButtonDrag::canDecode(e));
}

void ButtonSource::dragEnterEvent(QDragEnterEvent *e)
{
    e->setAccepted(ButtonDrag::canDecode(e));
}

void ButtonSource::dropEvent(QDropEvent *e)
{
    if (ButtonDrag::canDecode(e)) {
        emit dropped();
        e->accept();
    } else {
        e->ignore();
    }
}

void ButtonSource::mousePressEvent(QMouseEvent *e)
{
    ButtonSourceItem *i = dynamic_cast<ButtonSourceItem*>(itemAt(e->pos()));

    if (i) {
        ButtonDrag *bd = new ButtonDrag(i->button());
        QDrag *drag = new QDrag(this);
        drag->setMimeData(bd);
        drag->setPixmap(bitmapPixmap(i->button().icon, palette().color(QPalette::WindowText)));
        drag->exec();
    }
}

ButtonDropSiteItem::ButtonDropSiteItem(const Button& btn)
    : m_button(btn)
{
}

ButtonDropSiteItem::~ButtonDropSiteItem()
{
}

Button ButtonDropSiteItem::button()
{
    return m_button;
}

int ButtonDropSiteItem::width()
{
//  return m_button.icon.width();
    return 20;
}

int ButtonDropSiteItem::height()
{
//  return m_button.icon.height();
    return 20;
}

void ButtonDropSiteItem::draw(QPainter *p, const QPalette& cg, const QRect &r)
{
    if (m_button.supported)
        p->setPen(cg.color(QPalette::WindowText));
    else
        p->setPen(cg.color(QPalette::Disabled, QPalette::WindowText));
    QBitmap &i = m_button.icon;
    p->drawPixmap(r.left() + (r.width() - i.width()) / 2, r.top() + (r.height() - i.height()) / 2, i);
}


ButtonDropSite::ButtonDropSite(QWidget* parent)
    : QFrame(parent),
      m_selected(0)
{
    setAcceptDrops(true);
    setFrameShape(WinPanel);
    setFrameShadow(Raised);
    setMinimumHeight(26);
    setMaximumHeight(26);
    setMinimumWidth(250);        // Ensure buttons will fit
    setCursor(Qt::OpenHandCursor);
}

ButtonDropSite::~ButtonDropSite()
{
    clearLeft();
    clearRight();
}

void ButtonDropSite::clearLeft()
{
    while (!buttonsLeft.isEmpty()) {
        ButtonDropSiteItem *item = buttonsLeft.first();
        if (removeButton(item)) {
            emit buttonRemoved(item->button().type);
            delete item;
        }
    }
}

void ButtonDropSite::clearRight()
{
    while (!buttonsRight.isEmpty()) {
        ButtonDropSiteItem *item = buttonsRight.first();
        if (removeButton(item)) {
            emit buttonRemoved(item->button().type);
            delete item;
        }
    }
}

void ButtonDropSite::dragMoveEvent(QDragMoveEvent* e)
{
    QPoint p = e->pos();
    if (leftDropArea().contains(p) || rightDropArea().contains(p) || buttonAt(p)) {
        e->accept();

        // 2 pixel wide drop visualizer...
        QRect r = contentsRect();
        int x = -1;
        if (leftDropArea().contains(p)) {
            x = leftDropArea().left();
        } else if (rightDropArea().contains(p)) {
            x = rightDropArea().right() + 1;
        } else {
            ButtonDropSiteItem *item = buttonAt(p);
            if (item) {
                if (p.x() < item->rect.left() + item->rect.width() / 2) {
                    x = item->rect.left();
                } else {
                    x = item->rect.right() + 1;
                }
            }
        }
        if (x != -1) {
            QRect tmpRect(x, r.y(), 2, r.height());
            if (tmpRect != m_oldDropVisualizer) {
                cleanDropVisualizer();
                m_oldDropVisualizer = tmpRect;
                update(tmpRect);
            }
        }

    } else {
        e->ignore();

        cleanDropVisualizer();
    }
}

void ButtonDropSite::cleanDropVisualizer()
{
    if (m_oldDropVisualizer.isValid()) {
        QRect rect = m_oldDropVisualizer;
        m_oldDropVisualizer = QRect(); // rect is invalid
        update(rect);
    }
}

void ButtonDropSite::dragEnterEvent(QDragEnterEvent* e)
{
    if (ButtonDrag::canDecode(e))
        e->accept();
}

void ButtonDropSite::dragLeaveEvent(QDragLeaveEvent* /* e */)
{
    cleanDropVisualizer();
}

void ButtonDropSite::dropEvent(QDropEvent* e)
{
    cleanDropVisualizer();

    QPoint p = e->pos();

    // collect information where to insert the dropped button
    ButtonList *buttonList = 0;
    int         buttonPosition;

    if (leftDropArea().contains(p)) {
        buttonList = &buttonsLeft;
        buttonPosition = buttonsLeft.size();
    } else if (rightDropArea().contains(p)) {
        buttonList = &buttonsRight;
        buttonPosition = 0;
    } else {
        ButtonDropSiteItem *aboveItem = buttonAt(p);
        if (!aboveItem)
            return; // invalid drop. hasn't occurred _over_ a button (or left/right dropArea), return...

        int pos;
        if (!getItemPos(aboveItem, buttonList, pos)) {
            // didn't find the aboveItem. unlikely to happen since buttonAt() already seems to have found
            // something valid. anyway...
            return;
        }

        // got the list and the aboveItem position. now determine if the item should be inserted
        // before aboveItem or after aboveItem.
        QRect aboveItemRect = aboveItem->rect;
        if (!aboveItemRect.isValid())
            return;

        if (p.x() < aboveItemRect.left() + aboveItemRect.width() / 2) {
            // insert before the item
            buttonPosition = pos;
        } else {
            buttonPosition = pos + 1;
        }
    }

    // know where to insert the button. now see if we can use an existing item (drag within the widget = move)
    // orneed to create a new one
    ButtonDropSiteItem *buttonItem = 0;
    if (e->source() == this && m_selected) {
        ButtonList *oldList = 0;
        int oldPos;
        if (getItemPos(m_selected, oldList, oldPos)) {
            if (oldPos == buttonPosition && oldList == buttonList)
                return; // button didn't change its position during the drag...

            oldList->removeAt(oldPos);
            buttonItem = m_selected;

            // If we're inserting to the right of oldPos, in the same list,
            // better adjust the index..
            if (buttonList == oldList && buttonPosition > oldPos)
                --buttonPosition;
        } else {
            return; // m_selected not found, return...
        }
    } else {
        // create new button from the drop object...
        Button btn;
        if (ButtonDrag::decode(e, btn)) {
            buttonItem = new ButtonDropSiteItem(btn);
        } else {
            return; // something has gone wrong while we were trying to decode the drop event
        }
    }

    // now the item can actually be inserted into the list! :)
    (*buttonList).insert(buttonPosition, buttonItem);
    emit buttonAdded(buttonItem->button().type);
    emit changed();
    recalcItemGeometry();
    update();
}

bool ButtonDropSite::getItemPos(ButtonDropSiteItem *item, ButtonList* &list, int &pos)
{
    if (!item)
        return false;

    pos = buttonsLeft.indexOf(item); // try the left list first...
    if (pos >= 0) {
        list = &buttonsLeft;
        return true;
    }

    pos = buttonsRight.indexOf(item); // try the right list...
    if (pos >= 0) {
        list = &buttonsRight;
        return true;
    }

    list = 0;
    pos  = -1;
    return false;
}

QRect ButtonDropSite::leftDropArea()
{
    // return a 10 pixel drop area...
    QRect r = contentsRect();

    int leftButtonsWidth = calcButtonListWidth(buttonsLeft);
    return QRect(r.left() + leftButtonsWidth, r.top(), 10, r.height());
}

QRect ButtonDropSite::rightDropArea()
{
    // return a 10 pixel drop area...
    QRect r = contentsRect();

    int rightButtonsWidth = calcButtonListWidth(buttonsRight);
    return QRect(r.right() - rightButtonsWidth - 10, r.top(), 10, r.height());
}

void ButtonDropSite::mousePressEvent(QMouseEvent* e)
{
    QDrag *drag = new QDrag(this);
    m_selected = buttonAt(e->pos());
    if (m_selected) {
        ButtonDrag *bd = new ButtonDrag(m_selected->button());
        drag->setMimeData(bd);
        drag->setPixmap(bitmapPixmap(m_selected->button().icon, palette().color(QPalette::WindowText)));
        drag->exec();
    }
}

void ButtonDropSite::resizeEvent(QResizeEvent*)
{
    recalcItemGeometry();
}

void ButtonDropSite::recalcItemGeometry()
{
    QRect r = contentsRect();

    // update the geometry of the items in the left button list
    int offset = r.left();
    for (ButtonList::const_iterator it = buttonsLeft.constBegin(); it != buttonsLeft.constEnd(); ++it) {
        int w = (*it)->width();
        (*it)->rect = QRect(offset, r.top(), w, (*it)->height());
        offset += w;
    }

    // the right button list...
    offset = r.right() - calcButtonListWidth(buttonsRight);
    for (ButtonList::const_iterator it = buttonsRight.constBegin(); it != buttonsRight.constEnd(); ++it) {
        int w = (*it)->width();
        (*it)->rect = QRect(offset, r.top(), w, (*it)->height());
        offset += w;
    }
}

ButtonDropSiteItem *ButtonDropSite::buttonAt(QPoint p)
{
    // try to find the item in the left button list
    for (ButtonList::const_iterator it = buttonsLeft.constBegin(); it != buttonsLeft.constEnd(); ++it) {
        if ((*it)->rect.contains(p)) {
            return *it;
        }
    }

    // try to find the item in the right button list
    for (ButtonList::const_iterator it = buttonsRight.constBegin(); it != buttonsRight.constEnd(); ++it) {
        if ((*it)->rect.contains(p)) {
            return *it;
        }
    }

    return 0;
}

bool ButtonDropSite::removeButton(ButtonDropSiteItem *item)
{
    if (!item)
        return false;

    // try to remove the item from the left button list
    if (buttonsLeft.removeAll(item) >= 1) {
        return true;
    }

    // try to remove the item from the right button list
    if (buttonsRight.removeAll(item) >= 1) {
        return true;
    }

    return false;
}

int ButtonDropSite::calcButtonListWidth(const ButtonList& btns)
{
    int w = 0;
    for (ButtonList::const_iterator it = btns.constBegin(); it != btns.constEnd(); ++it) {
        w += (*it)->width();
    }

    return w;
}

bool ButtonDropSite::removeSelectedButton()
{
    bool succ = removeButton(m_selected);
    if (succ) {
        emit buttonRemoved(m_selected->button().type);
        emit changed();
        delete m_selected;
        m_selected = 0;
        recalcItemGeometry();
        update(); // repaint...
    }

    return succ;
}

void ButtonDropSite::drawButtonList(QPainter *p, const ButtonList& btns, int offset)
{
    for (ButtonList::const_iterator it = btns.constBegin(); it != btns.constEnd(); ++it) {
        QRect itemRect = (*it)->rect;
        if (itemRect.isValid()) {
            (*it)->draw(p, palette(), itemRect);
        }
        offset += (*it)->width();
    }
}

void ButtonDropSite::paintEvent(QPaintEvent* /*pe*/)
{
    QPainter p(this);
    int leftoffset = calcButtonListWidth(buttonsLeft);
    int rightoffset = calcButtonListWidth(buttonsRight);
    int offset = 3;

    QRect r = contentsRect();

    // Shrink by 1
    r.translate(1 + leftoffset, 1);
    r.setWidth(r.width() - 2 - leftoffset - rightoffset);
    r.setHeight(r.height() - 2);

    drawButtonList(&p, buttonsLeft, offset);

    QColor c1(palette().color(QPalette::Mid));
    p.fillRect(r, c1);
    p.setPen(palette().color(QPalette::WindowText));
    p.setFont(KGlobalSettings::windowTitleFont());
    p.drawText(r.adjusted(4, 0, -4, 0), Qt::AlignLeft | Qt::AlignVCenter, i18n("KDE"));

    offset = geometry().width() - 3 - rightoffset;
    drawButtonList(&p, buttonsRight, offset);

    if (m_oldDropVisualizer.isValid()) {
        p.fillRect(m_oldDropVisualizer, Qt::Dense4Pattern);
    }
}

ButtonSourceItem::ButtonSourceItem(QListWidget * parent, const Button& btn)
    : QListWidgetItem(parent),
      m_button(btn)
{
    setButton(btn);
}

ButtonSourceItem::~ButtonSourceItem()
{
}

void ButtonSourceItem::setButton(const Button& btn)
{
    m_button = btn;
    if (btn.supported) {
        setText(btn.name);
        setIcon(bitmapPixmap(btn.icon, QApplication::palette().color(QPalette::Text)));
        setForeground(QApplication::palette().brush(QPalette::Text));
    } else {
        setText(i18n("%1 (unavailable)", btn.name));
        setIcon(bitmapPixmap(btn.icon, QApplication::palette().color(QPalette::Disabled, QPalette::Text)));
        setForeground(QApplication::palette().brush(QPalette::Disabled, QPalette::Text));
    }
}

Button ButtonSourceItem::button() const
{
    return m_button;
}


ButtonPositionWidget::ButtonPositionWidget(QWidget *parent)
    : QWidget(parent),
      m_factory(0)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QLabel* label = new QLabel(this);
    m_dropSite = new ButtonDropSite(this);
    label->setWordWrap(true);
    label->setText(i18n("To add or remove titlebar buttons, simply <i>drag</i> items "
                        "between the available item list and the titlebar preview. Similarly, "
                        "drag items within the titlebar preview to re-position them."));
    m_buttonSource = new ButtonSource(this);
    m_buttonSource->setObjectName(QLatin1String("button_source"));

    layout->addWidget(label);
    layout->addWidget(m_dropSite);
    layout->addWidget(m_buttonSource);

    connect(m_dropSite, SIGNAL(buttonAdded(QChar)), m_buttonSource, SLOT(hideButton(QChar)));
    connect(m_dropSite, SIGNAL(buttonRemoved(QChar)), m_buttonSource, SLOT(showButton(QChar)));
    connect(m_buttonSource, SIGNAL(dropped()), m_dropSite, SLOT(removeSelectedButton()));

    connect(m_dropSite, SIGNAL(changed()), SIGNAL(changed()));

    // insert all possible buttons into the source (backwards to keep the preferred order...)
    bool dummy;

    m_supportedButtons = "MSHIAX_FBLR";
#ifdef KWIN_BUILD_KAPPMENU
    KConfig config("kdeglobals", KConfig::FullConfig);
    KConfigGroup configGroup = config.group("Appmenu Style");
    QString style = configGroup.readEntry("Style", "InApplication");

    if (style == "ButtonVertical") {
        m_supportedButtons = "MNSHIAX_FBLR"; // support all buttons
        new ButtonSourceItem(m_buttonSource, getButton('N', dummy));
    }
#endif

    new ButtonSourceItem(m_buttonSource, getButton('R', dummy));
    new ButtonSourceItem(m_buttonSource, getButton('L', dummy));
    new ButtonSourceItem(m_buttonSource, getButton('B', dummy));
    new ButtonSourceItem(m_buttonSource, getButton('F', dummy));
    new ButtonSourceItem(m_buttonSource, getButton('X', dummy));
    new ButtonSourceItem(m_buttonSource, getButton('A', dummy));
    new ButtonSourceItem(m_buttonSource, getButton('I', dummy));
    new ButtonSourceItem(m_buttonSource, getButton('H', dummy));
    new ButtonSourceItem(m_buttonSource, getButton('S', dummy));
    new ButtonSourceItem(m_buttonSource, getButton('M', dummy));
    new ButtonSourceItem(m_buttonSource, getButton('_', dummy));
}

ButtonPositionWidget::~ButtonPositionWidget()
{
}

Button ButtonPositionWidget::getButton(QChar type, bool& success)
{
    success = true;

    if (type == 'R') {
        QBitmap bmp = QBitmap::fromData(QSize(resize_width, resize_height), resize_bits);
        bmp.createMaskFromColor(Qt::white);
        return Button(i18n("Resize"), bmp, 'R', false, m_supportedButtons.contains('R'));
    } else if (type == 'L') {
        QBitmap bmp = QBitmap::fromData(QSize(shade_width, shade_height), shade_bits);
        bmp.createMaskFromColor(Qt::white);
        return Button(i18n("Shade"), bmp, 'L', false, m_supportedButtons.contains('L'));
    } else if (type == 'B') {
        QBitmap bmp = QBitmap::fromData(QSize(keepbelowothers_width, keepbelowothers_height), keepbelowothers_bits);
        bmp.createMaskFromColor(Qt::white);
        return Button(i18n("Keep Below Others"), bmp, 'B', false, m_supportedButtons.contains('B'));
    } else if (type == 'F') {
        QBitmap bmp = QBitmap::fromData(QSize(keepaboveothers_width, keepaboveothers_height), keepaboveothers_bits);
        bmp.createMaskFromColor(Qt::white);
        return Button(i18n("Keep Above Others"), bmp, 'F', false, m_supportedButtons.contains('F'));
    } else if (type == 'X') {
        QBitmap bmp = QBitmap::fromData(QSize(close_width, close_height), close_bits);
        bmp.createMaskFromColor(Qt::white);
        return Button(i18n("Close"), bmp, 'X', false, m_supportedButtons.contains('X'));
    } else if (type == 'A') {
        QBitmap bmp = QBitmap::fromData(QSize(maximize_width, maximize_height), maximize_bits);
        bmp.createMaskFromColor(Qt::white);
        return Button(i18n("Maximize"), bmp, 'A', false, m_supportedButtons.contains('A'));
    } else if (type == 'I') {
        QBitmap bmp = QBitmap::fromData(QSize(minimize_width, minimize_height), minimize_bits);
        bmp.createMaskFromColor(Qt::white);
        return Button(i18n("Minimize"), bmp, 'I', false, m_supportedButtons.contains('I'));
    } else if (type == 'H') {
        QBitmap bmp = QBitmap::fromData(QSize(help_width, help_height), help_bits);
        bmp.createMaskFromColor(Qt::white);
        return Button(i18n("Help"), bmp, 'H', false, m_supportedButtons.contains('H'));
    } else if (type == 'S') {
        QBitmap bmp = QBitmap::fromData(QSize(onalldesktops_width, onalldesktops_height), onalldesktops_bits);
        bmp.createMaskFromColor(Qt::white);
        return Button(i18n("On All Desktops"), bmp, 'S', false, m_supportedButtons.contains('S'));
    } else if (type == 'M') {
        QBitmap bmp = QBitmap::fromData(QSize(menu_width, menu_height), menu_bits);
        bmp.createMaskFromColor(Qt::white);
        return Button(i18nc("Button showing window actions menu", "Window Menu"), bmp, 'M', false, m_supportedButtons.contains('M'));
#ifdef KWIN_BUILD_KAPPMENU
    } else if (type == 'N') {
        QBitmap bmp = QBitmap::fromData(QSize(menu_width, menu_height), menu_bits);
        bmp.createMaskFromColor(Qt::white);
        return Button(i18nc("Button showing application menu imported from dbusmenu", "Application Menu"), bmp, 'N', false, m_supportedButtons.contains('N'));
#endif
    } else if (type == '_') {
        QBitmap bmp = QBitmap::fromData(QSize(spacer_width, spacer_height), spacer_bits);
        bmp.createMaskFromColor(Qt::white);
        return Button(i18n("--- spacer ---"), bmp, '_', true, m_supportedButtons.contains('_'));
    } else {
        success = false;
        return Button();
    }
}

QString ButtonPositionWidget::buttonsLeft() const
{
    ButtonList btns = m_dropSite->buttonsLeft;
    QString btnString = "";
    for (ButtonList::const_iterator it = btns.constBegin(); it != btns.constEnd(); ++it) {
        btnString.append((*it)->button().type);
    }
    return btnString;
}

QString ButtonPositionWidget::buttonsRight() const
{
    ButtonList btns = m_dropSite->buttonsRight;
    QString btnString = "";
    for (ButtonList::const_iterator it = btns.constBegin(); it != btns.constEnd(); ++it) {
        btnString.append((*it)->button().type);
    }
    return btnString;
}

void ButtonPositionWidget::setButtonsLeft(const QString &buttons)
{
    // to keep the button lists consistent, first remove all left buttons, then add buttons again...
    m_dropSite->clearLeft();

    for (int i = 0; i < buttons.length(); ++i) {
        bool succ = false;
        Button btn = getButton(buttons[i], succ);
        if (succ) {
            m_dropSite->buttonsLeft.append(new ButtonDropSiteItem(btn));
            m_buttonSource->hideButton(btn.type);
        }
    }
    m_dropSite->recalcItemGeometry();
    m_dropSite->update();
}

void ButtonPositionWidget::setButtonsRight(const QString &buttons)
{
    // to keep the button lists consistent, first remove all left buttons, then add buttons again...
    m_dropSite->clearRight();

    for (int i = 0; i < buttons.length(); ++i) {
        bool succ = false;
        Button btn = getButton(buttons[i], succ);
        if (succ) {
            m_dropSite->buttonsRight.append(new ButtonDropSiteItem(btn));
            m_buttonSource->hideButton(btn.type);
        }
    }
    m_dropSite->recalcItemGeometry();
    m_dropSite->update();
}

} // namespace KWin

#include "buttons.moc"
// vim: ts=4
// kate: space-indent off; tab-width 4;
