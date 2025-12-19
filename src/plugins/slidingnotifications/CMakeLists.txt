kwin_add_builtin_effect(slidingnotifications)

target_sources(slidingnotifications PRIVATE
    main.cpp
    slidingnotifications.cpp
)

target_link_libraries(slidingnotifications PRIVATE
    kwin
)
