function(kwin_add_script name source)
    kpackage_install_package(${source} ${name} scripts kwin)

    # Copy the script to the build directory so one can run tests without prior
    # make install. FIXME: use add_custom_command.
    file(COPY ${source}/contents ${source}/metadata.json DESTINATION ${CMAKE_BINARY_DIR}/bin/kwin/scripts/${name})
endfunction()

add_subdirectory(desktopchangeosd)
add_subdirectory(minimizeall)
add_subdirectory(synchronizeskipswitcher)
add_subdirectory(videowall)
