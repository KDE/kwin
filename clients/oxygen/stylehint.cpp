/***************************************************************************
 *   Copyright (C) 2006-2007 by Thomas Lübking                             *
 *   thomas.luebking@web.de                                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QStyleOptionComboBox>
#include "oxygen.h"

using namespace Oxygen;

int OxygenStyle::styleHint ( StyleHint hint, const QStyleOption * option, const QWidget * widget, QStyleHintReturn * returnData ) const {
   switch (hint) {
   case SH_EtchDisabledText: // Disabled text is "etched" as it is on Windows.
   case SH_DitherDisabledText: //  
      return false;
//    case SH_GUIStyle: // The GUI style to use.
   case SH_ScrollBar_MiddleClickAbsolutePosition: // A boolean value. If true, middle clicking on a scroll bar causes the slider to jump to that position. If false, middle clicking is ignored.
      return true;
   case SH_ScrollBar_LeftClickAbsolutePosition: // A boolean value. If true, left clicking on a scroll bar causes the slider to jump to that position. If false, left clicking will behave as appropriate for each control.
      return false;
   case SH_ScrollBar_ScrollWhenPointerLeavesControl: // A boolean value. If true, when clicking a scroll bar SubControl, holding the mouse button down and moving the pointer outside the SubControl, the scroll bar continues to scroll. If false, the scollbar stops scrolling when the pointer leaves the SubControl.
      return true;
   case SH_TabBar_Alignment: // The alignment for tabs in a QTabWidget. Possible values are Qt::AlignLeft, Qt::AlignCenter and Qt::AlignRight.
      return Qt::AlignLeft;
   case SH_Header_ArrowAlignment: // The placement of the sorting indicator may appear in list or table headers. Possible values are Qt::Left or Qt::Right.
      return Qt::AlignRight;
   case SH_Slider_SnapToValue: // Sliders snap to values while moving, as they do on Windows.
      return true;
   case SH_Slider_SloppyKeyEvents: // Key presses handled in a sloppy manner, i.e., left on a vertical slider subtracts a line.
      return true;
   case SH_ProgressDialog_CenterCancelButton: // Center button on progress dialogs, like Motif, otherwise right aligned.
      return false;
   case SH_ProgressDialog_TextLabelAlignment: // Type Qt::Alignment. Text label alignment in progress dialogs; Qt::Center on windows, Qt::VCenter otherwise.
      return Qt::AlignCenter;
   case SH_PrintDialog_RightAlignButtons: // Right align buttons in the print dialog, as done on Windows.
      return true;
   case SH_MainWindow_SpaceBelowMenuBar: // One or two pixel space between the menubar and the dockarea, as done on Windows.
      return 0;
//    case SH_FontDialog_SelectAssociatedText: // Select the text in the line edit, or when selecting an item from the listbox, or when the line edit receives focus, as done on Windows.
//    case SH_Menu_AllowActiveAndDisabled: // Allows disabled menu items to be active.
   case SH_Menu_SpaceActivatesItem: // Pressing the space bar activates the item, as done on Motif.
      return true;
//    case SH_Menu_SubMenuPopupDelay: // The number of milliseconds to wait before opening a submenu (256 on windows, 96 on Motif).
   case SH_Menu_Scrollable: // Whether popup menus must support scrolling.
      return true;
   case SH_Menu_SloppySubMenus: // Whether popupmenu's must support sloppy submenu; as implemented on Mac OS.
      return true;
   case SH_ScrollView_FrameOnlyAroundContents: // Whether scrollviews draw their frame only around contents (like Motif), or around contents, scroll bars and corner widgets (like Windows).
      return (!(widget && widget->inherits("QComboBoxListView")));
//       return true; // find solution for round corner end
   case SH_MenuBar_AltKeyNavigation: // Menu bars items are navigable by pressing Alt, followed by using the arrow keys to select the desired item.
      return true;
   case SH_ComboBox_ListMouseTracking: // Mouse tracking in combobox drop-down lists.
   case SH_Menu_MouseTracking: // Mouse tracking in popup menus.
   case SH_MenuBar_MouseTracking: // Mouse tracking in menubars.
      return true;
   case SH_Menu_FillScreenWithScroll: // Whether scrolling popups should fill the screen as they are scrolled.
      return false;
   case SH_ItemView_ChangeHighlightOnFocus: // Gray out selected items when losing focus.
      return true;
//    case SH_Widget_ShareActivation: // Turn on sharing activation with floating modeless dialogs.
   case SH_TabBar_SelectMouseType: // Which type of mouse event should cause a tab to be selected.
      return QEvent::MouseButtonRelease;
//    case SH_Q3ListViewExpand_SelectMouseType: // Which type of mouse event should cause a list view expansion to be selected.
   case SH_TabBar_PreferNoArrows: // Whether a tabbar should suggest a size to prevent scoll arrows.
      return false;
   case SH_ComboBox_Popup: // Allows popups as a combobox drop-down menu.
      return false;
   case SH_ComboBox_PopupFrameStyle:
      return QFrame::NoFrame; //QFrame::StyledPanel | QFrame::Plain;
//    case SH_Workspace_FillSpaceOnMaximize: // The workspace should maximize the client area.
//    case SH_TitleBar_NoBorder: // The title bar has no border.
   case SH_ScrollBar_StopMouseOverSlider: // Stops auto-repeat when the slider reaches the mouse position.
      return false;
   case SH_BlinkCursorWhenTextSelected: // Whether cursor should blink when text is selected.
      return false;
//    case SH_RichText_FullWidthSelection: // Whether richtext selections should extend to the full width of the document.
   case SH_GroupBox_TextLabelVerticalAlignment: // How to vertically align a groupbox's text label.
      return Qt::AlignTop;
//    case SH_GroupBox_TextLabelColor: // How to paint a groupbox's text label.
//    case SH_DialogButtons_DefaultButton: // Which button gets the default status in a dialog's button widget.
   case SH_DialogButtonLayout:
       return QDialogButtonBox::KdeLayout; //TODO: this should check what session type we are in
   case SH_ToolBox_SelectedPageTitleBold: // Boldness of the selected page title in a QToolBox.
      return true;
   case SH_LineEdit_PasswordCharacter: // The Unicode character to be used for passwords.
      return 42;
//    case SH_Table_GridLineColor: //  
   case SH_UnderlineShortcut: // Whether shortcuts are underlined.
      return true;
   case SH_SpinBox_AnimateButton: // Animate a click when up or down is pressed in a spin box.
      return true;
///    case SH_SpinBox_KeyPressAutoRepeatRate: // Auto-repeat interval for spinbox key presses.
///    case SH_SpinBox_ClickAutoRepeatRate: // Auto-repeat interval for spinbox mouse clicks.
   case SH_ToolTipLabel_Opacity: // An integer indicating the opacity for the tip label, 0 is completely transparent, 255 is completely opaque.
      return 160;
   case SH_DrawMenuBarSeparator: // Indicates whether or not the menubar draws separators.
      return false;
   case SH_TitleBar_ModifyNotification: // Indicates if the titlebar should show a '*' for windows that are modified.
      return true;
      //TODO: häää???
///    case SH_Button_FocusPolicy: // The default focus policy for buttons.
   case SH_MenuBar_DismissOnSecondClick: // A boolean indicating if a menu in the menubar should be dismissed when it is clicked on a second time. (Example: Clicking and releasing on the File Menu in a menubar and then immediately clicking on the File Menu again.)
      return true;
      //TODO: häää???
///    case SH_MessageBox_UseBorderForButtonSpacing: // A boolean indicating what the to use the border of the buttons (computed as half the button height) for the spacing of the button in a message box.
   case SH_TitleBar_AutoRaise: // A boolean indicating whether controls on a title bar ought to update when the mouse is over them.
      return true;
   case SH_ToolButton_PopupDelay: // An int indicating the popup delay in milliseconds for menus attached to tool buttons.
      return 300;
///    case SH_FocusFrame_Mask: // The mask of the focus frame.
///    case SH_RubberBand_Mask: // The mask of the rubber band.
///    case SH_WindowFrame_Mask: // The mask of the window frame.
   case SH_SpinControls_DisableOnBounds: // Determines if the spin controls will shown as disabled when reaching the spin range boundary.
      return true;
///    case SH_Dial_BackgroundRole: // Defines the style's preferred background role (as QPalette::ColorRole) for a dial widget.

#warning commenting those two line to make it compile. check.
//   case SH_ScrollBar_BackgroundMode: // The backgroundMode() for a scroll bar.
//      return ((widget && widget->inherits("QComboBoxListView"))) ?
//          Qt::PaletteBase :
//          Qt::PaletteBackground;

///    case SH_ComboBox_LayoutDirection: // The layout direction for the combo box. By default it should be the same value as the QStyleOption's direction.
///    case SH_ItemView_EllipsisLocation: // The location where ellipses should be added for item text that is too long to fit in an view item.
///    case SH_ItemView_ShowDecorationSelected: // When an item in an item view is selected, also highlight the branch or other decoration.
   case SH_ItemView_ActivateItemOnSingleClick: // Emit the activated signal when the user single clicks on an item in an item in an item view. Otherwise the signal is emitted when the user double clicks on an item.
      return true;
   case SH_WizardStyle:
      return 2;//QWizard::MacStyle;
   default:
      return QCommonStyle::styleHint(hint, option, widget, returnData);
   } // switch
}
