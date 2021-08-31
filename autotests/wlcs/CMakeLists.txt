add_library(kwin_wlcs MODULE
    main.cpp
    wlcs.cpp
    wlcs_pointer.cpp
    wlcs_touch.cpp
)
target_link_libraries(kwin_wlcs
    KWinIntegrationTestFramework
)