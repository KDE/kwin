set(systembell_SOURCES
    systembell.cpp
    systembell.qrc
    main.cpp
)

kwin_add_builtin_effect(systembell ${systembell_SOURCES})
target_link_libraries(systembell PRIVATE
    kwin

    KF6::GlobalAccel
    KF6::I18n
    Canberra::Canberra
)

if (KWIN_BUILD_X11)
    target_link_libraries(systembell PRIVATE XCB::XKB)
endif()
