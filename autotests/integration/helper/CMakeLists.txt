add_executable(copy copy.cpp copy.h)
target_link_libraries(copy Qt::Gui)
ecm_mark_as_test(copy)
######################
add_executable(paste paste.cpp paste.h)
target_link_libraries(paste Qt::Gui)
ecm_mark_as_test(paste)
######################
add_executable(kill kill.cpp kill.h)
target_link_libraries(kill Qt::Widgets)
ecm_mark_as_test(kill)
