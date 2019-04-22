set(CTEST_OUTPUT_ON_FAILURE ON)

function(build_test name)
	add_executable(${name} ${name}.cc ${SOURCES})
	target_link_libraries(${name} archive ${CMAKE_THREAD_LIBS_INIT})
endfunction()

function(test_prog name)
	build_test(${name})
	add_test(${name} ${name})
endfunction()

test_prog(noar)
