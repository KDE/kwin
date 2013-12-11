include_directories(${KDEBASE_WORKSPACE_SOURCE_DIR}/libs/oxygen)

########### next target ###############
set(oxygen_shadow_demo_SOURCES
    oxygenshadowdemodialog.cpp
    oxygenshadowdemowidget.cpp
    main.cpp)

set(oxygen_shadow_demo_FORMS ui/oxygenshadowdemo.ui)

qt5_wrap_ui(oxygen_shadow_demo_FORMS_HEADERS ${oxygen_shadow_demo_FORMS})

add_executable(oxygen-shadow-demo ${oxygen_shadow_demo_SOURCES} ${oxygen_shadow_demo_FORMS_HEADERS})

target_link_libraries(oxygen-shadow-demo Qt5::Core Qt5::Gui Qt5::Widgets Qt5::DBus)
target_link_libraries(oxygen-shadow-demo KF5::I18n)
target_link_libraries(oxygen-shadow-demo oxygenstyle )

if(X11_FOUND)
  target_link_libraries(oxygen-shadow-demo ${X11_LIBRARIES})
endif()

install(TARGETS oxygen-shadow-demo ${INSTALL_TARGETS_DEFAULT_ARGS})
