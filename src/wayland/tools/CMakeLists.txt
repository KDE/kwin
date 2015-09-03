include(ECMMarkAsTest)

find_package(Qt5Concurrent ${QT_MIN_VERSION} CONFIG QUIET)

if (Qt5Concurrent_FOUND)
    set(scannerSRCS generator.cpp)

    add_executable(kwaylandScanner ${scannerSRCS})
    target_link_libraries(kwaylandScanner Qt5::Core Qt5::Concurrent)
    ecm_mark_as_test(kwaylandScanner)

endif()
