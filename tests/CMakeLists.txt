set(CTEST_OUTPUT_ON_FAILURE ON)

function(build_test name)
	add_executable(${name} ${name}.cc)
	target_link_libraries(${name} ${LIBS})
endfunction()

function(test_prog name)
	build_test(${name})
	add_test(${name} ${name})
endfunction()

test_prog(noar)

build_test(re822)
add_test(822 ${CMAKE_CURRENT_SOURCE_DIR}/runtests ./re822 ${CMAKE_CURRENT_SOURCE_DIR}/822)
