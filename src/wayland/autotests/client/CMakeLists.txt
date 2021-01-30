########################################################
# Test WaylandOutput
########################################################
set( testWaylandOutput_SRCS
        test_wayland_output.cpp
    )
add_executable(testWaylandOutput ${testWaylandOutput_SRCS})
target_link_libraries( testWaylandOutput Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer Wayland::Client Wayland::Server)
add_test(NAME kwayland-testWaylandOutput COMMAND testWaylandOutput)
ecm_mark_as_test(testWaylandOutput)

########################################################
# Test WaylandSurface
########################################################
set( testWaylandSurface_SRCS
        test_wayland_surface.cpp
    )
add_executable(testWaylandSurface ${testWaylandSurface_SRCS})
target_link_libraries( testWaylandSurface Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer Wayland::Client Wayland::Server)
add_test(NAME kwayland-testWaylandSurface COMMAND testWaylandSurface)
ecm_mark_as_test(testWaylandSurface)

########################################################
# Test WaylandSeat
########################################################
if (HAVE_LINUX_INPUT_H)
    set( testWaylandSeat_SRCS
            test_wayland_seat.cpp
        )
    add_executable(testWaylandSeat ${testWaylandSeat_SRCS})
    target_link_libraries( testWaylandSeat Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer Wayland::Client Wayland::Server)
    add_test(NAME kwayland-testWaylandSeat COMMAND testWaylandSeat)
    ecm_mark_as_test(testWaylandSeat)
endif()

########################################################
# Test ShmPool
########################################################
set( testShmPool_SRCS
        test_shm_pool.cpp
    )
add_executable(testShmPool ${testShmPool_SRCS})
target_link_libraries( testShmPool Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer)
add_test(NAME kwayland-testShmPool COMMAND testShmPool)
ecm_mark_as_test(testShmPool)

########################################################
# Test KWin OutputManagement
########################################################
set( test_wayland_outputmanagement_SRCS
        test_wayland_outputmanagement.cpp
    )
add_executable(testWaylandOutputManagement ${test_wayland_outputmanagement_SRCS})
target_link_libraries( testWaylandOutputManagement Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer Wayland::Client)
add_test(NAME kwayland-testWaylandOutputManagement COMMAND testWaylandOutputManagement)
ecm_mark_as_test(testWaylandOutputManagement)

########################################################
# Test KWin OutputDevice
########################################################
set( test_wayland_outputdevice_SRCS
        test_wayland_outputdevice.cpp
    )
add_executable(testWaylandOutputDevice ${test_wayland_outputdevice_SRCS})
target_link_libraries( testWaylandOutputDevice Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer Wayland::Client)
add_test(NAME kwayland-testWaylandOutputDevice COMMAND testWaylandOutputDevice)
ecm_mark_as_test(testWaylandOutputDevice)

########################################################
# Test Compositor
########################################################
set( testCompositor_SRCS
        test_compositor.cpp
    )
add_executable(testCompositor ${testCompositor_SRCS})
target_link_libraries( testCompositor Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer)
add_test(NAME kwayland-testCompositor COMMAND testCompositor)
ecm_mark_as_test(testCompositor)

########################################################
# Test SubCompositor
########################################################
set( testSubCompositor_SRCS
        test_wayland_subcompositor.cpp
    )
add_executable(testSubCompositor ${testSubCompositor_SRCS})
target_link_libraries( testSubCompositor Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer)
add_test(NAME kwayland-testSubCompositor COMMAND testSubCompositor)
ecm_mark_as_test(testSubCompositor)


########################################################
# Test SubSurface
########################################################
set( testSubSurface_SRCS
        test_wayland_subsurface.cpp
    )
add_executable(testSubSurface ${testSubSurface_SRCS})
target_link_libraries( testSubSurface Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer Wayland::Client)
add_test(NAME kwayland-testSubSurface COMMAND testSubSurface)
ecm_mark_as_test(testSubSurface)

########################################################
# Test Region
########################################################
set( testRegion_SRCS
        test_wayland_region.cpp
    )
add_executable(testRegion ${testRegion_SRCS})
target_link_libraries( testRegion Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer)
add_test(NAME kwayland-testRegion COMMAND testRegion)
ecm_mark_as_test(testRegion)

########################################################
# Test Blur
########################################################
set( testBlur_SRCS
        test_wayland_blur.cpp
    )
add_executable(testBlur ${testBlur_SRCS})
target_link_libraries( testBlur Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer)
add_test(NAME kwayland-testBlur COMMAND testBlur)
ecm_mark_as_test(testBlur)

########################################################
# Test Contrast
########################################################
set( testContrast_SRCS
        test_wayland_contrast.cpp
    )
add_executable(testContrast ${testContrast_SRCS})
target_link_libraries( testContrast Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer Wayland::Client)
add_test(NAME kwayland-testContrast COMMAND testContrast)
ecm_mark_as_test(testContrast)

########################################################
# Test Slide
########################################################
set( testSlide_SRCS
        test_wayland_slide.cpp
    )
add_executable(testSlide ${testSlide_SRCS})
target_link_libraries( testSlide Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer Wayland::Client)
add_test(NAME kwayland-testSlide COMMAND testSlide)
ecm_mark_as_test(testSlide)

########################################################
# Test Window Management
########################################################
set( testWindowmanagement_SRCS
        test_wayland_windowmanagement.cpp
    )
add_executable(testWindowmanagement ${testWindowmanagement_SRCS})
target_link_libraries( testWindowmanagement Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer Wayland::Client)
add_test(NAME kwayland-testWindowmanagement COMMAND testWindowmanagement)
ecm_mark_as_test(testWindowmanagement)

########################################################
# Test DataSource
########################################################
set( testDataSource_SRCS
        test_datasource.cpp
    )
add_executable(testDataSource ${testDataSource_SRCS})
target_link_libraries( testDataSource Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer Wayland::Client)
add_test(NAME kwayland-testDataSource COMMAND testDataSource)
ecm_mark_as_test(testDataSource)

########################################################
# Test DataDevice
########################################################
set( testDataDevice_SRCS
        test_datadevice.cpp
    )
add_executable(testDataDevice ${testDataDevice_SRCS})
target_link_libraries( testDataDevice Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer Wayland::Client)
add_test(NAME kwayland-testDataDevice COMMAND testDataDevice)
ecm_mark_as_test(testDataDevice)

########################################################
# Test ServerSideDecoration
########################################################
set( testServerSideDecoration_SRCS
        test_server_side_decoration.cpp
    )
add_executable(testServerSideDecoration ${testServerSideDecoration_SRCS})
target_link_libraries( testServerSideDecoration Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer Wayland::Client)
add_test(NAME kwayland-testServerSideDecoration COMMAND testServerSideDecoration)
ecm_mark_as_test(testServerSideDecoration)

########################################################
# Test Drag'N'Drop
########################################################
set( testDragAndDrop_SRCS
        test_drag_drop.cpp
    )
add_executable(testDragAndDrop ${testDragAndDrop_SRCS})
target_link_libraries( testDragAndDrop Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer Wayland::Client)
add_test(NAME kwayland-testDragAndDrop COMMAND testDragAndDrop)
ecm_mark_as_test(testDragAndDrop)

########################################################
# Test PlasmaShell
########################################################
set( testPlasmaShell_SRCS
        test_plasmashell.cpp
    )
add_executable(testPlasmaShell ${testPlasmaShell_SRCS})
target_link_libraries( testPlasmaShell Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer Wayland::Client)
add_test(NAME kwayland-testPlasmaShell COMMAND testPlasmaShell)
ecm_mark_as_test(testPlasmaShell)

########################################################
# Test Idle
########################################################
set( testIdle_SRCS
        test_idle.cpp
    )
add_executable(testIdle ${testIdle_SRCS})
target_link_libraries( testIdle Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer)
add_test(NAME kwayland-testIdle COMMAND testIdle)
ecm_mark_as_test(testIdle)

########################################################
# Test Shadow
########################################################
set( testShadow_SRCS
        test_shadow.cpp
    )
add_executable(testShadow ${testShadow_SRCS})
target_link_libraries( testShadow Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer)
add_test(NAME kwayland-testShadow COMMAND testShadow)
ecm_mark_as_test(testShadow)

########################################################
# Test FakeInput
########################################################
if (HAVE_LINUX_INPUT_H)
    set( testFakeInput_SRCS
            test_fake_input.cpp
        )
    add_executable(testFakeInput ${testFakeInput_SRCS})
    target_link_libraries( testFakeInput Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer)
    add_test(NAME kwayland-testFakeInput COMMAND testFakeInput)
    ecm_mark_as_test(testFakeInput)
endif()

########################################################
# Test PlasmaWindowModel
########################################################
if (HAVE_LINUX_INPUT_H)
    set( testPlasmaWindowModel_SRCS
            test_plasma_window_model.cpp
        )
    add_executable(testPlasmaWindowModel ${testPlasmaWindowModel_SRCS})
    target_link_libraries( testPlasmaWindowModel Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer)
    add_test(NAME kwayland-testPlasmaWindowModel COMMAND testPlasmaWindowModel)
    ecm_mark_as_test(testPlasmaWindowModel)
endif()

########################################################
# Test TextInputV2
########################################################
set( testTextInputV2_SRCS
        test_text_input_v2.cpp
    )
add_executable(testTextInputV2 ${testTextInputV2_SRCS})
target_link_libraries( testTextInputV2 Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer)
add_test(NAME kwayland-testTextInputV2 COMMAND testTextInputV2)
ecm_mark_as_test(testTextInputV2)

########################################################
# Test Error
########################################################
set( testError_SRCS
        test_error.cpp
    )
add_executable(testError ${testError_SRCS})
target_link_libraries( testError Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer Wayland::Client)
add_test(NAME kwayland-testError COMMAND testError)
ecm_mark_as_test(testError)

########################################################
# Test Selection
########################################################
set( testSelection_SRCS
        test_selection.cpp
    )
add_executable(testSelection ${testSelection_SRCS})
target_link_libraries( testSelection Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer Wayland::Client)
add_test(NAME kwayland-testSelection COMMAND testSelection)
ecm_mark_as_test(testSelection)

########################################################
# Test XdgForeign
########################################################
set( testXdgForeign_SRCS
      test_xdg_foreign.cpp
      )
add_executable(testXdgForeign ${testXdgForeign_SRCS})
target_link_libraries( testXdgForeign Qt::Test Qt::Gui Plasma::KWaylandServer KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testXdgForeign COMMAND testXdgForeign)
ecm_mark_as_test(testXdgForeign)

########################################################
# Test XdgShell
########################################################
set(testXdgShell_SRCS test_xdg_shell.cpp)
add_executable(testXdgShell ${testXdgShell_SRCS})
target_link_libraries( testXdgShell Qt::Test Qt::Gui Plasma::KWaylandServer KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testXdgShell COMMAND testXdgShell)
ecm_mark_as_test(testXdgShell)

########################################################
# Test Pointer Constraints
########################################################
add_executable(testPointerConstraints test_pointer_constraints.cpp)
target_link_libraries( testPointerConstraints Qt::Test Qt::Gui Plasma::KWaylandServer KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testPointerConstraints COMMAND testPointerConstraints)
ecm_mark_as_test(testPointerConstraints)


########################################################
# Test Filter
########################################################
set( testFilter_SRCS
        test_wayland_filter.cpp
    )
add_executable(testFilter ${testFilter_SRCS})
target_link_libraries( testFilter Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer Wayland::Server)
add_test(NAME kwayland-testFilter COMMAND testFilter)
ecm_mark_as_test(testFilter)

########################################################
# Test Appmenu
########################################################
set( testAppmenu_SRCS
        test_wayland_appmenu.cpp
    )
add_executable(testAppmenu ${testAppmenu_SRCS})
target_link_libraries( testAppmenu Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer)
add_test(NAME kwayland-testAppmenu COMMAND testAppmenu)
ecm_mark_as_test(testAppmenu)

########################################################
# Test Appmenu
########################################################
set( testServerSideDecorationPalette_SRCS
        test_server_side_decoration_palette.cpp
    )
add_executable(testServerSideDecorationPalette ${testServerSideDecorationPalette_SRCS})
target_link_libraries( testServerSideDecorationPalette Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer)
add_test(NAME kwayland-testServerSideDecorationPalette COMMAND testServerSideDecorationPalette)
ecm_mark_as_test(testServerSideDecorationPalette)

########################################################
# Test VirtualDesktop
########################################################
set( testPlasmaVirtualDesktop_SRCS
        test_plasma_virtual_desktop.cpp
    )
add_executable(testPlasmaVirtualDesktop ${testPlasmaVirtualDesktop_SRCS})
target_link_libraries( testPlasmaVirtualDesktop Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer)
add_test(NAME kwayland-testPlasmaVirtualDesktop COMMAND testPlasmaVirtualDesktop)
ecm_mark_as_test(testPlasmaVirtualDesktop)

########################################################
# Test XDG Output
########################################################
set( testXdgOutput_SRCS
        test_xdg_output.cpp
    )
add_executable(testXdgOutput ${testXdgOutput_SRCS})
target_link_libraries( testXdgOutput Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer Wayland::Client Wayland::Server)
add_test(NAME kwayland-testXdgOutput COMMAND testXdgOutput)
ecm_mark_as_test(testXdgOutput)

########################################################
# Test XDG Decoration
########################################################
set( testXdgdecoration_SRCS
        test_xdg_decoration.cpp
    )
add_executable(testXdgDecoration ${testXdgdecoration_SRCS})
target_link_libraries( testXdgDecoration Qt::Test Qt::Gui KF5::WaylandClient Plasma::KWaylandServer Wayland::Client Wayland::Server)
add_test(NAME kwayland-testXdgDecoration COMMAND testXdgDecoration)
ecm_mark_as_test(testXdgDecoration)

