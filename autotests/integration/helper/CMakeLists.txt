add_executable(copy copy.cpp)
target_link_libraries(copy Qt::Gui)
ecm_mark_as_test(copy)
######################
add_executable(paste paste.cpp)
target_link_libraries(paste Qt::Gui)
ecm_mark_as_test(paste)
######################
add_executable(kill kill.cpp)
target_link_libraries(kill Qt::Widgets)
ecm_mark_as_test(kill)
