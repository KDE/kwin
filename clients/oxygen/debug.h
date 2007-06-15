#define PRINTDEVICE(p) qWarning("device is %s", (p->device()->devType() == QInternal::Widget) ?\
"Widget": (p->device()->devType() == QInternal::Pixmap) ?\
"Pixmap": (p->device()->devType() == QInternal::Printer) ?\
"Printer": (p->device()->devType() == QInternal::Picture) ?\
"Picture": (p->device()->devType() == QInternal::UndefinedDevice) ?\
"UndefinedDevice": "fuckdevice!" );

#define PRINTFLAGS(f) qWarning("Style Flags:\n%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",\
f & QStyle::Style_Default ? "Default, " : "",\
f & QStyle::Style_Enabled ? "Enabled, " : "",\
f & QStyle::Style_Raised ? "Raised, " : "",\
f & QStyle::Style_Sunken ? "Sunken, " : "",\
f & QStyle::Style_Off ? "Off, " : "",\
f & QStyle::Style_NoChange ? "NoChange, " : "",\
f & QStyle::Style_On ? "On, " : "",\
f & QStyle::Style_Down ? "Down, " : "",\
f & QStyle::Style_Horizontal ? "Horizontal, " : "",\
f & QStyle::Style_HasFocus ? "HasFocus, " : "",\
f & QStyle::Style_Top ? "Top, " : "",\
f & QStyle::Style_Bottom ? "Bottom, " : "",\
f & QStyle::Style_FocusAtBorder ? "FocusAtBorder, " : "",\
f & QStyle::Style_AutoRaise ? "AutoRaise, " : "",\
f & QStyle::Style_MouseOver ? "MouseOver, " : "",\
f & QStyle::Style_Up ? "Style_Up, " : "",\
f & QStyle::Style_Selected ? "Selected, " : "",\
f & QStyle::Style_HasFocus ? "HasFocus, " : "",\
f & QStyle::Style_Active ? "Active, " : "",\
f & QStyle::Style_ButtonDefault ? "ButtonDefault" : "" )

#define PRINTEVENT(e) qWarning("Event: %s",\
e->type() == QEvent::Timer                    ? " Timer " : \
e->type() == QEvent::MouseButtonPress         ? " MouseButtonPress " : \
e->type() == QEvent::MouseButtonRelease       ? " MouseButtonRelease " : \
e->type() == QEvent::MouseButtonDblClick      ? " MouseButtonDblClick " : \
e->type() == QEvent::MouseMove                ? " MouseMove " : \
e->type() == QEvent::KeyPress                 ? " KeyPress " : \
e->type() == QEvent::KeyRelease               ? " KeyRelease " : \
e->type() == QEvent::FocusIn                  ? " FocusIn " : \
e->type() == QEvent::FocusOut                 ? " FocusOut " : \
e->type() == QEvent::Enter                    ? " Enter " : \
e->type() == QEvent::Leave                    ? " Leave " : \
e->type() == QEvent::Paint                    ? " Paint " : \
e->type() == QEvent::Move                     ? " Move " : \
e->type() == QEvent::Resize                   ? " Resize " : \
e->type() == QEvent::Create                   ? " Create " : \
e->type() == QEvent::Destroy                  ? " Destroy " : \
e->type() == QEvent::Show                     ? " Show " : \
e->type() == QEvent::Hide                     ? " Hide " : \
e->type() == QEvent::Close                    ? " Close " : \
e->type() == QEvent::Quit                     ? " Quit " : \
e->type() == QEvent::Reparent                 ? " Reparent " : \
e->type() == QEvent::ShowMinimized            ? " ShowMinimized " : \
e->type() == QEvent::ShowNormal               ? " ShowNormal " : \
e->type() == QEvent::WindowActivate           ? " WindowActivate " : \
e->type() == QEvent::WindowDeactivate         ? " WindowDeactivate " : \
e->type() == QEvent::ShowToParent             ? " ShowToParent " : \
e->type() == QEvent::HideToParent             ? " HideToParent " : \
e->type() == QEvent::ShowMaximized            ? " ShowMaximized " : \
e->type() == QEvent::ShowFullScreen           ? " ShowFullScreen " : \
e->type() == QEvent::Accel                    ? " Accel " : \
e->type() == QEvent::Wheel                    ? " Wheel " : \
e->type() == QEvent::AccelAvailable           ? " AccelAvailable " : \
e->type() == QEvent::CaptionChange            ? " CaptionChange " : \
e->type() == QEvent::IconChange               ? " IconChange " : \
e->type() == QEvent::ParentFontChange         ? " ParentFontChange " : \
e->type() == QEvent::ApplicationFontChange    ? " ApplicationFontChange " : \
e->type() == QEvent::ParentPaletteChange      ? " ParentPaletteChange " : \
e->type() == QEvent::ApplicationPaletteChange ? " ApplicationPaletteChange " : \
e->type() == QEvent::PaletteChange            ? " PaletteChange " : \
e->type() == QEvent::Clipboard                ? " Clipboard " : \
e->type() == QEvent::Speech                   ? " Speech " : \
e->type() == QEvent::SockAct                  ? " SockAct " : \
e->type() == QEvent::AccelOverride            ? " AccelOverride " : \
e->type() == QEvent::DeferredDelete           ? " DeferredDelete " : \
e->type() == QEvent::DragEnter                ? " DragEnter " : \
e->type() == QEvent::DragMove                 ? " DragMove " : \
e->type() == QEvent::DragLeave                ? " DragLeave " : \
e->type() == QEvent::Drop                     ? " Drop " : \
e->type() == QEvent::DragResponse             ? " DragResponse " : \
e->type() == QEvent::ChildInserted            ? " ChildInserted " : \
e->type() == QEvent::ChildRemoved             ? " ChildRemoved " : \
e->type() == QEvent::LayoutHint               ? " LayoutHint " : \
e->type() == QEvent::ShowWindowRequest        ? " ShowWindowRequest " : \
e->type() == QEvent::WindowBlocked            ? " WindowBlocked " : \
e->type() == QEvent::WindowUnblocked          ? " WindowUnblocked " : \
e->type() == QEvent::ActivateControl          ? " ActivateControl " : \
e->type() == QEvent::DeactivateControl        ? " DeactivateControl " : \
e->type() == QEvent::ContextMenu              ? " ContextMenu " : \
e->type() == QEvent::IMStart                  ? " IMStart " : \
e->type() == QEvent::IMCompose                ? " IMCompose " : \
e->type() == QEvent::IMEnd                    ? " IMEnd " : \
e->type() == QEvent::Accessibility            ? " Accessibility " : \
e->type() == QEvent::TabletMove               ? " TabletMove " : \
e->type() == QEvent::LocaleChange             ? " LocaleChange " : \
e->type() == QEvent::LanguageChange           ? " LanguageChange " : \
e->type() == QEvent::LayoutDirectionChange    ? " LayoutDirectionChange " : \
e->type() == QEvent::Style                    ? " Style " : \
e->type() == QEvent::TabletPress              ? " TabletPress " : \
e->type() == QEvent::TabletRelease            ? " TabletRelease " : \
e->type() == QEvent::OkRequest                ? " OkRequest " : \
e->type() == QEvent::HelpRequest              ? " HelpRequest " : \
e->type() == QEvent::WindowStateChange        ? " WindowStateChange " : \
e->type() == QEvent::IconDrag                 ? " IconDrag " : "Unknown Event");

#define PRINTCOMPLEXCONTROL(cc) qWarning("Complex Control: %s",\
QStyle::CC_SpinWidget   ? " QStyle::CC_SpinWidget " : \
QStyle::CC_ComboBox     ? " CC_ComboBox  " : \
QStyle::CC_ScrollBar    ? " CC_ScrollBar  " : \
QStyle::CC_Slider       ? " CC_Slider " : \
QStyle::CC_ToolButton   ? " CC_ToolButton  " : \
QStyle::CC_TitleBar     ? "CC_TitleBar  " : \
QStyle::CC_ListView     ? "CC_ListView " : "Unknow Control");

#define _IDENTIFYOBJECT_(o) qWarning("%s (%s)%s%s (%s)", o->name(), o->className(), o->parent() ? " is child of " : " has no daddy", o->parent() ? o->parent()->name() : "", o->parent() ? o->parent()->className() : "!")

#define _COMPARECOLORS_(c1,c2) qWarning("%d/%d/%d vs. %d/%d/%d", c1.red(), c1.green(), c1.blue(), c2.red(), c2.green(), c2.blue());

// #define MOUSEDEBUG
#undef MOUSEDEBUG

//#define FUNCTIONALBG
 #undef FUNCTIONALBG

#include <qtimer.h>
#define _PROFILESTART_ QTime timer; int time; timer.start();
#define _PROFILERESTART_ timer.restart();
#define _PROFILESTOP_(_STRING_) time = timer.elapsed(); qWarning("%s: %d",_STRING_,time);
