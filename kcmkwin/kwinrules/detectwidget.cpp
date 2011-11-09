/*
 * Copyright (c) 2004 Lubos Lunak <l.lunak@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <KDialog>
#include "detectwidget.h"

#include <kapplication.h>
#include <klocale.h>
#include <kdebug.h>
#include <kwindowsystem.h>
#include <QLabel>
#include <QRadioButton>
#include <QCheckBox>
//Added by qt3to4:
#include <QMouseEvent>
#include <QEvent>
#include <QByteArray>
#include <QTimer>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <fixx11h.h>
#include <QX11Info>

namespace KWin
{

DetectWidget::DetectWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
}

DetectDialog::DetectDialog(QWidget* parent, const char* name)
    : KDialog(parent),
      grabber(NULL)
{
    setObjectName(name);
    setModal(true);
    setButtons(Ok | Cancel);

    widget = new DetectWidget(this);
    setMainWidget(widget);
}

void DetectDialog::detect(WId window, int secs)
{
    if (window == 0)
        QTimer::singleShot(secs*1000, this, SLOT(selectWindow()));
    else
        readWindow(window);
}

void DetectDialog::readWindow(WId w)
{
    if (w == 0) {
        emit detectionDone(false);
        return;
    }
    info = KWindowSystem::windowInfo(w, -1U, -1U);   // read everything
    if (!info.valid()) {
        emit detectionDone(false);
        return;
    }
    wmclass_class = info.windowClassClass();
    wmclass_name = info.windowClassName();
    role = info.windowRole();
    type = info.windowType(NET::NormalMask | NET::DesktopMask | NET::DockMask
                           | NET::ToolbarMask | NET::MenuMask | NET::DialogMask | NET::OverrideMask | NET::TopMenuMask
                           | NET::UtilityMask | NET::SplashMask);
    title = info.name();
    machine = info.clientMachine();
    executeDialog();
}

void DetectDialog::executeDialog()
{
    static const char* const types[] = {
        I18N_NOOP("Normal Window"),
        I18N_NOOP("Desktop"),
        I18N_NOOP("Dock (panel)"),
        I18N_NOOP("Toolbar"),
        I18N_NOOP("Torn-Off Menu"),
        I18N_NOOP("Dialog Window"),
        I18N_NOOP("Override Type"),
        I18N_NOOP("Standalone Menubar"),
        I18N_NOOP("Utility Window"),
        I18N_NOOP("Splash Screen")
    };
    widget->class_label->setText(wmclass_class + " (" + wmclass_name + ' ' + wmclass_class + ')');
    widget->role_label->setText(role);
    widget->match_role->setEnabled(!role.isEmpty());
    if (type == NET::Unknown)
        widget->type_label->setText(i18n("Unknown - will be treated as Normal Window"));
    else
        widget->type_label->setText(i18n(types[ type ]));
    widget->title_label->setText(title);
    widget->machine_label->setText(machine);
    widget->adjustSize();
    adjustSize();
    if (width() < 4*height()/3)
        resize(4*height()/3, height());
    emit detectionDone(exec() == KDialog::Accepted);
}

QByteArray DetectDialog::selectedClass() const
{
    if (widget->match_whole_class->isChecked())
        return wmclass_name + ' ' + wmclass_class;
    return wmclass_class;
}

bool DetectDialog::selectedWholeClass() const
{
    return widget->match_whole_class->isChecked();
}

QByteArray DetectDialog::selectedRole() const
{
    if (widget->match_role->isChecked())
        return role;
    return "";
}

QString DetectDialog::selectedTitle() const
{
    return title;
}

Rules::StringMatch DetectDialog::titleMatch() const
{
    return widget->match_title->isChecked() ? Rules::ExactMatch : Rules::UnimportantMatch;
}

bool DetectDialog::selectedWholeApp() const
{
    return !widget->match_type->isChecked();
}

NET::WindowType DetectDialog::selectedType() const
{
    return type;
}

QByteArray DetectDialog::selectedMachine() const
{
    return machine;
}

void DetectDialog::selectWindow()
{
    // use a dialog, so that all user input is blocked
    // use WX11BypassWM and moving away so that it's not actually visible
    // grab only mouse, so that keyboard can be used e.g. for switching windows
    grabber = new KDialog(0, Qt::X11BypassWindowManagerHint);
    grabber->move(-1000, -1000);
    grabber->setModal(true);
    grabber->show();
    grabber->grabMouse(Qt::CrossCursor);
    grabber->installEventFilter(this);
}

bool DetectDialog::eventFilter(QObject* o, QEvent* e)
{
    if (o != grabber)
        return false;
    if (e->type() != QEvent::MouseButtonRelease)
        return false;
    delete grabber;
    grabber = NULL;
    if (static_cast< QMouseEvent* >(e)->button() != Qt::LeftButton) {
        emit detectionDone(false);
        return true;
    }
    readWindow(findWindow());
    return true;
}

WId DetectDialog::findWindow()
{
    Window root;
    Window child;
    uint mask;
    int rootX, rootY, x, y;
    Window parent = QX11Info::appRootWindow();
    Atom wm_state = XInternAtom(QX11Info::display(), "WM_STATE", False);
    for (int i = 0;
            i < 10;
            ++i) {
        XQueryPointer(QX11Info::display(), parent, &root, &child,
                      &rootX, &rootY, &x, &y, &mask);
        if (child == None)
            return 0;
        Atom type;
        int format;
        unsigned long nitems, after;
        unsigned char* prop;
        if (XGetWindowProperty(QX11Info::display(), child, wm_state, 0, 0, False, AnyPropertyType,
                              &type, &format, &nitems, &after, &prop) == Success) {
            if (prop != NULL)
                XFree(prop);
            if (type != None)
                return child;
        }
        parent = child;
    }
    return 0;
}

} // namespace

#include "detectwidget.moc"
