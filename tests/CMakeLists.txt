set(CTEST_OUTPUT_ON_FAILURE ON)
set(TD ${CMAKE_CURRENT_SOURCE_DIR})

function(build_test name)
	add_executable(${name} ${name}.cc)
	target_link_libraries(${name} ${LIBS})
endfunction()

function(test_prog name)
	build_test(${name})
	add_test(${name} ${name})
endfunction()

add_test(harness ${TD}/runtests ${TD}/harness/strcmp ${TD}/harness)

test_prog(noar)

build_test(re822)
add_test(822 ${TD}/runtests ./re822 ${TD}/822)

add_test(bad_debs ${TD}/runtests ${TD}/unp_deb ${TD}/bad_debs)
add_test(compress ${TD}/runtests "${TD}/unp_deb -" ${TD}/compress)
