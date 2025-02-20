integrationTest(NAME testScripting SRCS scripting_test.cpp)
integrationTest(NAME testScriptingScreenEdge SRCS screenedge_test.cpp)
integrationTest(NAME testMinimizeAllScript SRCS minimizeall_test.cpp LIBS KF6::Package)
