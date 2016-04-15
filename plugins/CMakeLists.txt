add_subdirectory(kglobalaccel)
add_subdirectory(qpa)
add_subdirectory(idletime)
add_subdirectory(platforms)

if(KWIN_BUILD_DECORATIONS)
    add_subdirectory(kdecorations)
endif()
