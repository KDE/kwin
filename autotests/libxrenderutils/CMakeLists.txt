add_executable(blendPictureTest blendpicture_test.cpp)
set_target_properties(blendPictureTest PROPERTIES COMPILE_DEFINITIONS "NO_NONE_WINDOW")
add_test(NAME xrenderutils-blendPictureTest COMMAND blendPictureTest)
target_link_libraries(blendPictureTest
    kwinxrenderutils
    Qt::Test
    Qt::Gui
    XCB::XCB
    XCB::RENDER
    XCB::XFIXES
)
if (QT_MAJOR_VERSION EQUAL "5")
    target_link_libraries(blendPictureTest Qt::X11Extras)
else()
    target_link_libraries(blendPictureTest Qt::GuiPrivate)
endif()
ecm_mark_as_test(blendPictureTest)
