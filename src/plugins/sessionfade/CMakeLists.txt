kwin_add_builtin_effect(sessionfade main.cpp sessionfade.cpp)
target_link_libraries(sessionfade PRIVATE
    kwin

    PW::KScreenLocker
)
