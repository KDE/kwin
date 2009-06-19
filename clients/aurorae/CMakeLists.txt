project(aurorae)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules )

find_package(KDE4 4.2.92 REQUIRED)
find_package(KDE4Workspace REQUIRED)
include (KDE4Defaults)
include (MacroLibrary)

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories (${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES})

add_subdirectory(src)
add_subdirectory(themes/example-deco)

