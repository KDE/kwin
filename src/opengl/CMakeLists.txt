
add_executable(shadercompiler shadercompiler.cpp)
target_link_libraries(shadercompiler spirv-cross-glsl)

function(add_shader_file TARGET SHADER_TARGET_NAME SHADER_SOURCE_FILE)
    add_custom_target(${SHADER_TARGET_NAME} ALL
        COMMAND shadercompiler "${CMAKE_CURRENT_LIST_DIR}/${SHADER_SOURCE_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_SOURCE_FILE}"
        SOURCES ${SHADER_SOURCE_FILES}
    )
    add_dependencies(${TARGET} ${SHADER_TARGET_NAME})
endfunction()

add_shader_file(kwin basicvert "basic.vert")
add_shader_file(kwin basicfrag "basic.frag")
