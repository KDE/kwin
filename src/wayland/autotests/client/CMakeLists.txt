########################################################
# Test WaylandConnectionThread
########################################################
set( testWaylandConnectionThread_SRCS
        test_wayland_connection_thread.cpp
    )
add_executable(testWaylandConnectionThread ${testWaylandConnectionThread_SRCS})
target_link_libraries( testWaylandConnectionThread Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client Wayland::Server)
add_test(NAME kwayland-testWaylandConnectionThread COMMAND testWaylandConnectionThread)
ecm_mark_as_test(testWaylandConnectionThread)

########################################################
# Test WaylandRegistry
########################################################
set( testWaylandRegistry_SRCS
        test_wayland_registry.cpp
    )
add_executable(testWaylandRegistry ${testWaylandRegistry_SRCS})
target_link_libraries( testWaylandRegistry Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client Wayland::Server)
add_test(NAME kwayland-testWaylandRegistry COMMAND testWaylandRegistry)
ecm_mark_as_test(testWaylandRegistry)

########################################################
# Test WaylandFullscreenShell
########################################################
if(Wayland_VERSION VERSION_GREATER "1.4.0")
  find_program(WESTON_EXECUTABLE weston DOC "Path to the weston executable.")
  if(WESTON_EXECUTABLE)
    set( testWaylandFullscreenShell_SRCS
            test_wayland_fullscreen_shell.cpp
        )
    add_executable(testWaylandFullscreenShell ${testWaylandFullscreenShell_SRCS})
    target_link_libraries( testWaylandFullscreenShell Qt5::Test KF5::WaylandClient Wayland::Client)
    add_test(NAME kwayland-testWaylandFullscreenShell COMMAND testWaylandFullscreenShell)
    ecm_mark_as_test(testWaylandFullscreenShell)
  else()
    message(STATUS "The weston executable was not found. Some autotests will not be executed.")
  endif()
endif()

########################################################
# Test WaylandOutput
########################################################
set( testWaylandOutput_SRCS
        test_wayland_output.cpp
    )
add_executable(testWaylandOutput ${testWaylandOutput_SRCS})
target_link_libraries( testWaylandOutput Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client Wayland::Server)
add_test(NAME kwayland-testWaylandOutput COMMAND testWaylandOutput)
ecm_mark_as_test(testWaylandOutput)

########################################################
# Test WaylandShell
########################################################
set( testWaylandShell_SRCS
        test_wayland_shell.cpp
    )
add_executable(testWaylandShell ${testWaylandShell_SRCS})
target_link_libraries( testWaylandShell Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client Wayland::Server)
add_test(NAME kwayland-testWaylandShell COMMAND testWaylandShell)
ecm_mark_as_test(testWaylandShell)

########################################################
# Test WaylandSurface
########################################################
set( testWaylandSurface_SRCS
        test_wayland_surface.cpp
    )
add_executable(testWaylandSurface ${testWaylandSurface_SRCS})
target_link_libraries( testWaylandSurface Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client Wayland::Server)
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
    target_link_libraries( testWaylandSeat Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client Wayland::Server)
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
target_link_libraries( testShmPool Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer)
add_test(NAME kwayland-testShmPool COMMAND testShmPool)
ecm_mark_as_test(testShmPool)

########################################################
# Test KWin OutputManagement
########################################################
set( test_wayland_outputmanagement_SRCS
        test_wayland_outputmanagement.cpp
    )
add_executable(testWaylandOutputManagement ${test_wayland_outputmanagement_SRCS})
target_link_libraries( testWaylandOutputManagement Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client)
add_test(NAME kwayland-testWaylandOutputManagement COMMAND testWaylandOutputManagement)
ecm_mark_as_test(testWaylandOutputManagement)

########################################################
# Test KWin OutputDevice
########################################################
set( test_wayland_outputdevice_SRCS
        test_wayland_outputdevice.cpp
    )
add_executable(testWaylandOutputDevice ${test_wayland_outputdevice_SRCS})
target_link_libraries( testWaylandOutputDevice Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client)
add_test(NAME kwayland-testWaylandOutputDevice COMMAND testWaylandOutputDevice)
ecm_mark_as_test(testWaylandOutputDevice)

########################################################
# Test Compositor
########################################################
set( testCompositor_SRCS
        test_compositor.cpp
    )
add_executable(testCompositor ${testCompositor_SRCS})
target_link_libraries( testCompositor Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer)
add_test(NAME kwayland-testCompositor COMMAND testCompositor)
ecm_mark_as_test(testCompositor)

########################################################
# Test SubCompositor
########################################################
set( testSubCompositor_SRCS
        test_wayland_subcompositor.cpp
    )
add_executable(testSubCompositor ${testSubCompositor_SRCS})
target_link_libraries( testSubCompositor Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer)
add_test(NAME kwayland-testSubCompositor COMMAND testSubCompositor)
ecm_mark_as_test(testSubCompositor)


########################################################
# Test SubSurface
########################################################
set( testSubSurface_SRCS
        test_wayland_subsurface.cpp
    )
add_executable(testSubSurface ${testSubSurface_SRCS})
target_link_libraries( testSubSurface Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client)
add_test(NAME kwayland-testSubSurface COMMAND testSubSurface)
ecm_mark_as_test(testSubSurface)

########################################################
# Test Region
########################################################
set( testRegion_SRCS
        test_wayland_region.cpp
    )
add_executable(testRegion ${testRegion_SRCS})
target_link_libraries( testRegion Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer)
add_test(NAME kwayland-testRegion COMMAND testRegion)
ecm_mark_as_test(testRegion)

########################################################
# Test Blur
########################################################
set( testBlur_SRCS
        test_wayland_blur.cpp
    )
add_executable(testBlur ${testBlur_SRCS})
target_link_libraries( testBlur Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer)
add_test(NAME kwayland-testBlur COMMAND testBlur)
ecm_mark_as_test(testBlur)

########################################################
# Test Contrast
########################################################
set( testContrast_SRCS
        test_wayland_contrast.cpp
    )
add_executable(testContrast ${testContrast_SRCS})
target_link_libraries( testContrast Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client)
add_test(NAME kwayland-testContrast COMMAND testContrast)
ecm_mark_as_test(testContrast)

########################################################
# Test Slide
########################################################
set( testSlide_SRCS
        test_wayland_slide.cpp
    )
add_executable(testSlide ${testSlide_SRCS})
target_link_libraries( testSlide Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client)
add_test(NAME kwayland-testSlide COMMAND testSlide)
ecm_mark_as_test(testSlide)

########################################################
# Test Window Management
########################################################
set( testWindowmanagement_SRCS
        test_wayland_windowmanagement.cpp
    )
add_executable(testWindowmanagement ${testWindowmanagement_SRCS})
target_link_libraries( testWindowmanagement Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client)
add_test(NAME kwayland-testWindowmanagement COMMAND testWindowmanagement)
ecm_mark_as_test(testWindowmanagement)

########################################################
# Test DataSource
########################################################
set( testDataSource_SRCS
        test_datasource.cpp
    )
add_executable(testDataSource ${testDataSource_SRCS})
target_link_libraries( testDataSource Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client)
add_test(NAME kwayland-testDataSource COMMAND testDataSource)
ecm_mark_as_test(testDataSource)

########################################################
# Test DataDevice
########################################################
set( testDataDevice_SRCS
        test_datadevice.cpp
    )
add_executable(testDataDevice ${testDataDevice_SRCS})
target_link_libraries( testDataDevice Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client)
add_test(NAME kwayland-testDataDevice COMMAND testDataDevice)
ecm_mark_as_test(testDataDevice)

########################################################
# Test ServerSideDecoration
########################################################
set( testServerSideDecoration_SRCS
        test_server_side_decoration.cpp
    )
add_executable(testServerSideDecoration ${testServerSideDecoration_SRCS})
target_link_libraries( testServerSideDecoration Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client)
add_test(NAME kwayland-testServerSideDecoration COMMAND testServerSideDecoration)
ecm_mark_as_test(testServerSideDecoration)

########################################################
# Test Drag'N'Drop
########################################################
set( testDragAndDrop_SRCS
        test_drag_drop.cpp
    )
add_executable(testDragAndDrop ${testDragAndDrop_SRCS})
target_link_libraries( testDragAndDrop Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client)
add_test(NAME kwayland-testDragAndDrop COMMAND testDragAndDrop)
ecm_mark_as_test(testDragAndDrop)

########################################################
# Test PlasmaShell
########################################################
set( testPlasmaShell_SRCS
        test_plasmashell.cpp
    )
add_executable(testPlasmaShell ${testPlasmaShell_SRCS})
target_link_libraries( testPlasmaShell Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client)
add_test(NAME kwayland-testPlasmaShell COMMAND testPlasmaShell)
ecm_mark_as_test(testPlasmaShell)

########################################################
# Test Idle
########################################################
set( testIdle_SRCS
        test_idle.cpp
    )
add_executable(testIdle ${testIdle_SRCS})
target_link_libraries( testIdle Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer)
add_test(NAME kwayland-testIdle COMMAND testIdle)
ecm_mark_as_test(testIdle)

########################################################
# Test Shadow
########################################################
set( testShadow_SRCS
        test_shadow.cpp
    )
add_executable(testShadow ${testShadow_SRCS})
target_link_libraries( testShadow Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer)
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
    target_link_libraries( testFakeInput Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer)
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
    target_link_libraries( testPlasmaWindowModel Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer)
    add_test(NAME kwayland-testPlasmaWindowModel COMMAND testPlasmaWindowModel)
    ecm_mark_as_test(testPlasmaWindowModel)
endif()

########################################################
# Test TextInput
########################################################
set( testTextInput_SRCS
        test_text_input.cpp
    )
add_executable(testTextInput ${testTextInput_SRCS})
target_link_libraries( testTextInput Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer)
add_test(NAME kwayland-testTextInput COMMAND testTextInput)
ecm_mark_as_test(testTextInput)

########################################################
# Test Error
########################################################
set( testError_SRCS
        test_error.cpp
    )
add_executable(testError ${testError_SRCS})
target_link_libraries( testError Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client)
add_test(NAME kwayland-testError COMMAND testError)
ecm_mark_as_test(testError)

########################################################
# Test Selection
########################################################
set( testSelection_SRCS
        test_selection.cpp
    )
add_executable(testSelection ${testSelection_SRCS})
target_link_libraries( testSelection Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client)
add_test(NAME kwayland-testSelection COMMAND testSelection)
ecm_mark_as_test(testSelection)

########################################################
# Test XdgShellV5
########################################################
set( testXdgShellV5_SRCS
      test_xdg_shell.cpp
      test_xdg_shell_v5.cpp
      )
add_executable(testXdgShellV5 ${testXdgShellV5_SRCS})
target_link_libraries( testXdgShellV5 Qt5::Test Qt5::Gui KF5::WaylandServer KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testXdgShellV5 COMMAND testXdgShellV5)
ecm_mark_as_test(testXdgShellV5)

########################################################
# Test XdgForeign
########################################################
set( testXdgForeign_SRCS
      test_xdg_foreign.cpp
      )
add_executable(testXdgForeign ${testXdgForeign_SRCS})
target_link_libraries( testXdgForeign Qt5::Test Qt5::Gui KF5::WaylandServer KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testXdgForeign COMMAND testXdgForeign)
ecm_mark_as_test(testXdgForeign)

########################################################
# Test XdgShellV6
########################################################
set( testXdgShellV6_SRCS
      test_xdg_shell.cpp
      test_xdg_shell_v6.cpp
      )
add_executable(testXdgShellV6 ${testXdgShellV6_SRCS})
target_link_libraries( testXdgShellV6 Qt5::Test Qt5::Gui KF5::WaylandServer KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testXdgShellV6 COMMAND testXdgShellV6)
ecm_mark_as_test(testXdgShellV6)
########################################################
# Test XdgShellStable
########################################################
set( testXdgShellStable_SRCS
      test_xdg_shell.cpp
      test_xdg_shell_stable.cpp
      )
add_executable(testXdgShellStable ${testXdgShellStable_SRCS})
target_link_libraries( testXdgShellStable Qt5::Test Qt5::Gui KF5::WaylandServer KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testXdgShellStable COMMAND testXdgShellStable)
ecm_mark_as_test(testXdgShellStable)

########################################################
# Test Pointer Constraints
########################################################
add_executable(testPointerConstraints test_pointer_constraints.cpp)
target_link_libraries( testPointerConstraints Qt5::Test Qt5::Gui KF5::WaylandServer KF5::WaylandClient Wayland::Client)
add_test(NAME kwayland-testPointerConstraints COMMAND testPointerConstraints)
ecm_mark_as_test(testPointerConstraints)


########################################################
# Test Filter
########################################################
set( testFilter_SRCS
        test_wayland_filter.cpp
    )
add_executable(testFilter ${testFilter_SRCS})
target_link_libraries( testFilter Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Server)
add_test(NAME kwayland-testFilter COMMAND testFilter)
ecm_mark_as_test(testFilter)

########################################################
# Test Appmenu
########################################################
set( testAppmenu_SRCS
        test_wayland_appmenu.cpp
    )
add_executable(testAppmenu ${testAppmenu_SRCS})
target_link_libraries( testAppmenu Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer)
add_test(NAME kwayland-testAppmenu COMMAND testAppmenu)
ecm_mark_as_test(testAppmenu)

########################################################
# Test Appmenu
########################################################
set( testServerSideDecorationPalette_SRCS
        test_server_side_decoration_palette.cpp
    )
add_executable(testServerSideDecorationPalette ${testServerSideDecorationPalette_SRCS})
target_link_libraries( testServerSideDecorationPalette Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer)
add_test(NAME kwayland-testServerSideDecorationPalette COMMAND testServerSideDecorationPalette)
ecm_mark_as_test(testServerSideDecorationPalette)

########################################################
# Test RemoteAccess
########################################################
set( testRemoteAccess_SRCS
        test_remote_access.cpp
    )
add_executable(testRemoteAccess ${testRemoteAccess_SRCS})
target_link_libraries( testRemoteAccess Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer)
add_test(NAME kwayland-testRemoteAccess COMMAND testRemoteAccess)
ecm_mark_as_test(testRemoteAccess)

########################################################
# Test VirtualDesktop
########################################################
set( testPlasmaVirtualDesktop_SRCS
        test_plasma_virtual_desktop.cpp
    )
add_executable(testPlasmaVirtualDesktop ${testPlasmaVirtualDesktop_SRCS})
target_link_libraries( testPlasmaVirtualDesktop Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer)
add_test(NAME kwayland-testPlasmaVirtualDesktop COMMAND testPlasmaVirtualDesktop)
ecm_mark_as_test(testPlasmaVirtualDesktop)

########################################################
# Test XDG Output
########################################################
set( testXdgOutput_SRCS
        test_xdg_output.cpp
    )
add_executable(testXdgOutput ${testXdgOutput_SRCS})
target_link_libraries( testXdgOutput Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client Wayland::Server)
add_test(NAME kwayland-testXdgOutput COMMAND testXdgOutput)
ecm_mark_as_test(testXdgOutput)

########################################################
# Test XDG Decoration
########################################################
set( testXdgdecoration_SRCS
        test_xdg_decoration.cpp
    )
add_executable(testXdgDecoration ${testXdgdecoration_SRCS})
target_link_libraries( testXdgDecoration Qt5::Test Qt5::Gui KF5::WaylandClient KF5::WaylandServer Wayland::Client Wayland::Server)
add_test(NAME kwayland-testXdgDecoration COMMAND testXdgDecoration)
ecm_mark_as_test(testXdgDecoration)

