remove_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_STRICT_ITERATORS -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_KEYWORDS)

add_subdirectory(common)

add_subdirectory(animations)
add_subdirectory(options)
add_subdirectory(decoration)
add_subdirectory(rules)
add_subdirectory(screenedges)
add_subdirectory(scripts)
add_subdirectory(desktop)
add_subdirectory(effects)
add_subdirectory(virtualkeyboard)
add_subdirectory(xwayland)

if (KWIN_BUILD_TABBOX)
    add_subdirectory(tabbox)
endif()
