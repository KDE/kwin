add_executable(kill kill.cpp)
target_link_libraries(kill Qt::Widgets)
ecm_mark_as_test(kill)
