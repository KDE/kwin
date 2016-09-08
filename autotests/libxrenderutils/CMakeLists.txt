add_executable(blendPictureTest blendpicture_test.cpp)
add_test(xrenderutils-blendPictureTest blendPictureTest)
target_link_libraries(blendPictureTest
    kwinxrenderutils
    Qt5::Test
    Qt5::Gui
    Qt5::X11Extras
    XCB::XCB
    XCB::RENDER
    XCB::XFIXES
    )
ecm_mark_as_test(blendPictureTest)
