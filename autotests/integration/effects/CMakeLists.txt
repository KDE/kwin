if (XCB_ICCCM_FOUND)
    integrationTest(NAME testTranslucency SRCS translucency_test.cpp LIBS XCB::ICCCM)
endif()
