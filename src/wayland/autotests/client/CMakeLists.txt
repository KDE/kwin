########################################################
# Test WaylandOutput
########################################################
set( testWaylandOutput_SRCS
        test_wayland_output.cpp
        ../../tests/fakeoutput.cpp
    )
add_executable(testWaylandOutput ${testWaylandOutput_SRCS})
target_link_libraries( testWaylandOutput Qt::Test Qt::Gui KF5::WaylandClient kwin Wayland::Client Wayland::Server)
add_test(NAME kwayland-testWaylandOutput COMMAND testWaylandOutput)
ecm_mark_as_test(testWaylandOutput)

########################################################
# Test WaylandSurface
########################################################
set( testWaylandSurface_SRCS
        test_wayland_surface.cpp
        ../../tests/fakeoutput.cpp
    )
add_executable(testWaylandSurface ${testWaylandSurface_SRCS})
target_link_libraries( testWaylandSurface Qt::Test Qt::Gui KF5::WaylandClient kwin Wayland::Client Wayland::Server)
add_test(NAME kwayland-testWaylandSurface COMMAND testWaylandSurface)
ecm_mark_as_test(testWaylandSurface)

########################################################
# Test WaylandSeat
########################################################
add_executable(testWaylandSeat)
set( testWaylandSeat_SRCS
        test_wayland_seat.cpp
    )
if (QT_MAJOR_VERSION EQUAL "5")
    ecm_add_qtwayland_client_protocol(testWaylandSeat_SRCS
        PROTOCOL ${WaylandProtocols_DATADIR}/unstable/pointer-gestures/pointer-gestures-unstable-v1.xml
        BASENAME pointer-gestures-unstable-v1
    )
else()
    qt6_generate_wayland_protocol_client_sources(testWaylandSeat FILES
        ${WaylandProtocols_DATADIR}/unstable/pointer-gestures/pointer-gestures-unstable-v1.xml)
endif()
target_sources(testWaylandSeat PRIVATE ${testWaylandSeat_SRCS})
target_link_libraries( testWaylandSeat Qt::Test Qt::Gui KF5::WaylandClient kwin Wayland::Client Wayland::Server)
add_test(NAME kwayland-testWaylandSeat COMMAND testWaylandSeat)
ecm_mark_as_test(testWaylandSeat)

########################################################
# Test ShmPool
########################################################
set( testShmPool_SRCS
        test_shm_pool.cpp
    )
add_executable(testShmPool ${testShmPool_SRCS})
target_link_libraries( testShmPool Qt::Test Qt::Gui KF5::WaylandClient kwin)
add_test(NAME kwayland-testShmPool COMMAND testShmPool)
ecm_mark_as_test(testShmPool)

########################################################
# Test SubSurface
########################################################
set( testSubSurface_SRCS
        test_wayland_subsurface.cpp
    )
add_executable(testSubSurface ${testSubSurface_SRCS})
target_link_libraries( testSubSurface Qt::Test Qt::Gui KF5::WaylandClient kwin Wayland::Client)
add_test(NAME kwayland-testSubSurface COMMAND testSubSurface)
ecm_mark_as_test(testSubSurface)

########################################################
# Test Blur
########################################################
set( testBlur_SRCS
        test_wayland_blur.cpp
    )
add_executable(testBlur ${testBlur_SRCS})
target_link_libraries( testBlur Qt::Test Qt::Gui KF5::WaylandClient kwin)
add_test(NAME kwayland-testBlur COMMAND testBlur)
ecm_mark_as_test(testBlur)

########################################################
# Test Contrast
########################################################
set( testContrast_SRCS
        test_wayland_contrast.cpp
    )
add_executable(testContrast ${testContrast_SRCS})
target_link_libraries( testContrast Qt::Test Qt::Gui KF5::WaylandClient kwin Wayland::Client)
add_test(NAME kwayland-testContrast COMMAND testContrast)
ecm_mark_as_test(testContrast)

########################################################
# Test Slide
########################################################
set( testSlide_SRCS
        test_wayland_slide.cpp
    )
add_executable(testSlide ${testSlide_SRCS})
target_link_libraries( testSlide Qt::Test Qt::Gui KF5::WaylandClient kwin Wayland::Client)
add_test(NAME kwayland-testSlide COMMAND testSlide)
ecm_mark_as_test(testSlide)

########################################################
# Test Window Management
########################################################
set( testWindowmanagement_SRCS
        test_wayland_windowmanagement.cpp
    )
add_executable(testWindowmanagement ${testWindowmanagement_SRCS})
target_link_libraries( testWindowmanagement Qt::Test Qt::Gui KF5::WaylandClient kwin Wayland::Client)
add_test(NAME kwayland-testWindowmanagement COMMAND testWindowmanagement)
ecm_mark_as_test(testWindowmanagement)

########################################################
# Test DataSource
########################################################
set( testDataSource_SRCS
        test_datasource.cpp
    )
add_executable(testDataSource ${testDataSource_SRCS})
target_link_libraries( testDataSource Qt::Test Qt::Gui KF5::WaylandClient kwin Wayland::Client)
add_test(NAME kwayland-testDataSource COMMAND testDataSource)
ecm_mark_as_test(testDataSource)

########################################################
# Test DataDevice
########################################################
set( testDataDevice_SRCS
        test_datadevice.cpp
    )
add_executable(testDataDevice ${testDataDevice_SRCS})
target_link_libraries( testDataDevice Qt::Test Qt::Gui KF5::WaylandClient kwin Wayland::Client)
add_test(NAME kwayland-testDataDevice COMMAND testDataDevice)
ecm_mark_as_test(testDataDevice)

########################################################
# Test ServerSideDecoration
########################################################
set( testServerSideDecoration_SRCS
        test_server_side_decoration.cpp
    )
add_executable(testServerSideDecoration ${testServerSideDecoration_SRCS})
target_link_libraries( testServerSideDecoration Qt::Test Qt::Gui KF5::WaylandClient kwin Wayland::Client)
add_test(NAME kwayland-testServerSideDecoration COMMAND testServerSideDecoration)
ecm_mark_as_test(testServerSideDecoration)

########################################################
# Test Drag'N'Drop
########################################################
set( testDragAndDrop_SRCS
        test_drag_drop.cpp
    )
add_executable(testDragAndDrop ${testDragAndDrop_SRCS})
target_link_libraries( testDragAndDrop Qt::Test Qt::Gui KF5::WaylandClient kwin Wayland::Client)
add_test(NAME kwayland-testDragAndDrop COMMAND testDragAndDrop)
ecm_mark_as_test(testDragAndDrop)

########################################################
# Test PlasmaShell
########################################################
set( testPlasmaShell_SRCS
        test_plasmashell.cpp
    )
add_executable(testPlasmaShell ${testPlasmaShell_SRCS})
target_link_libraries( testPlasmaShell Qt::Test Qt::Gui KF5::WaylandClient kwin Wayland::Client)
add_test(NAME kwayland-testPlasmaShell COMMAND testPlasmaShell)
ecm_mark_as_test(testPlasmaShell)

########################################################
# Test Shadow
########################################################
set( testShadow_SRCS
        test_shadow.cpp
    )
add_executable(testShadow ${testShadow_SRCS})
target_link_libraries( testShadow Qt::Test Qt::Gui KF5::WaylandClient kwin)
add_test(NAME kwayland-testShadow COMMAND testShadow)
ecm_mark_as_test(testShadow)

########################################################
# Test FakeInput
########################################################
set( testFakeInput_SRCS
        test_fake_input.cpp
    )
add_executable(testFakeInput ${testFakeInput_SRCS})
target_link_libraries( testFakeInput Qt::Test Qt::Gui KF5::WaylandClient kwin)
add_test(NAME kwayland-testFakeInput COMMAND testFakeInput)
ecm_mark_as_test(testFakeInput)

########################################################
# Test TextInputV2
########################################################
set( testTextInputV2_SRCS
        test_text_input_v2.cpp
    )
add_executable(testTextInputV2 ${testTextInputV2_SRCS})
target_link_libraries( testTextInputV2 Qt::Test Qt::Gui KF5::WaylandClient kwin)
add_test(NAME kwayland-testTextInputV2 COMMAND testTextInputV2)
ecm_mark_as_test(testTextInputV2)

########################################################
# Test Error
########################################################
set( testError_SRCS
        test_error.cpp
    )
add_executable(testError ${testError_SRCS})
target_link_libraries( testError Qt::Test Qt::Gui KF5::WaylandClient kwin Wayland::Client)
add_test(NAME kwayland-testError COMMAND testError)
ecm_mark_as_test(testError)

########################################################
# Test Selection
########################################################
set( testSelection_SRCS
        test_selection.cpp
    )
add_executable(testSelection ${testSelection_SRCS})
target_link_libraries( testSelection Qt::Test Qt::Gui KF5::WaylandClient kwin Wayland::Client)
add_test(NAME kwayland-testSelection COMMAND testSelection)
ecm_mark_as_test(testSelection)

########################################################
# Test XdgForeign
########################################################
set( testXdgForeign_SRCS
      test_xdg_foreign.cpp
      )
add_executable(testXdgForeign ${testXdgForeign_SRCS})
target_link_libraries( testXdgForeign Qt::Test Qt::Gui kwin KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testXdgForeign COMMAND testXdgForeign)
ecm_mark_as_test(testXdgForeign)

########################################################
# Test XdgShell
########################################################
set(testXdgShell_SRCS test_xdg_shell.cpp ../../tests/fakeoutput.cpp)
add_executable(testXdgShell ${testXdgShell_SRCS})
target_link_libraries( testXdgShell Qt::Test Qt::Gui kwin KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testXdgShell COMMAND testXdgShell)
ecm_mark_as_test(testXdgShell)

########################################################
# Test Pointer Constraints
########################################################
add_executable(testPointerConstraintsInterface test_pointer_constraints.cpp)
target_link_libraries( testPointerConstraintsInterface Qt::Test Qt::Gui kwin KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testPointerConstraintsInterface COMMAND testPointerConstraintsInterface)
ecm_mark_as_test(testPointerConstraintsInterface)


########################################################
# Test Filter
########################################################
set( testFilter_SRCS
        test_wayland_filter.cpp
    )
add_executable(testFilter ${testFilter_SRCS})
target_link_libraries( testFilter Qt::Test Qt::Gui KF5::WaylandClient kwin Wayland::Server)
add_test(NAME kwayland-testFilter COMMAND testFilter)
ecm_mark_as_test(testFilter)

########################################################
# Test Appmenu
########################################################
set( testAppmenu_SRCS
        test_wayland_appmenu.cpp
    )
add_executable(testAppmenu ${testAppmenu_SRCS})
target_link_libraries( testAppmenu Qt::Test Qt::Gui KF5::WaylandClient kwin)
add_test(NAME kwayland-testAppmenu COMMAND testAppmenu)
ecm_mark_as_test(testAppmenu)

########################################################
# Test Appmenu
########################################################
set( testServerSideDecorationPalette_SRCS
        test_server_side_decoration_palette.cpp
    )
add_executable(testServerSideDecorationPalette ${testServerSideDecorationPalette_SRCS})
target_link_libraries( testServerSideDecorationPalette Qt::Test Qt::Gui KF5::WaylandClient kwin)
add_test(NAME kwayland-testServerSideDecorationPalette COMMAND testServerSideDecorationPalette)
ecm_mark_as_test(testServerSideDecorationPalette)

########################################################
# Test VirtualDesktop
########################################################
set( testPlasmaVirtualDesktop_SRCS
        test_plasma_virtual_desktop.cpp
    )
add_executable(testPlasmaVirtualDesktop ${testPlasmaVirtualDesktop_SRCS})
target_link_libraries( testPlasmaVirtualDesktop Qt::Test Qt::Gui KF5::WaylandClient kwin)
add_test(NAME kwayland-testPlasmaVirtualDesktop COMMAND testPlasmaVirtualDesktop)
ecm_mark_as_test(testPlasmaVirtualDesktop)

########################################################
# Test Activities
########################################################
set( testPlasmaActivities_SRCS
        test_plasma_activities.cpp
    )
add_executable(testPlasmaActivities ${testPlasmaActivities_SRCS})
target_link_libraries( testPlasmaActivities Qt::Test Qt::Gui KF5::WaylandClient kwin)
add_test(NAME kwayland-testPlasmaActivities COMMAND testPlasmaActivities)
ecm_mark_as_test(testPlasmaActivities)

########################################################
# Test XDG Output
########################################################
set( testXdgOutput_SRCS
        test_xdg_output.cpp
        ../../tests/fakeoutput.cpp
    )
add_executable(testXdgOutput ${testXdgOutput_SRCS})
target_link_libraries( testXdgOutput Qt::Test Qt::Gui KF5::WaylandClient kwin Wayland::Client Wayland::Server)
add_test(NAME kwayland-testXdgOutput COMMAND testXdgOutput)
ecm_mark_as_test(testXdgOutput)

########################################################
# Test XDG Decoration
########################################################
set( testXdgdecoration_SRCS
        test_xdg_decoration.cpp
    )
add_executable(testXdgDecoration ${testXdgdecoration_SRCS})
target_link_libraries( testXdgDecoration Qt::Test Qt::Gui KF5::WaylandClient kwin Wayland::Client Wayland::Server)
add_test(NAME kwayland-testXdgDecoration COMMAND testXdgDecoration)
ecm_mark_as_test(testXdgDecoration)

