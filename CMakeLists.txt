cmake_minimum_required(VERSION 3.3)
project(zdebootstrap CXX)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif (NOT CMAKE_BUILD_TYPE)

include(FindThreads)

set(SOURCES
	${CMAKE_SOURCE_DIR}/deb.cc
	${CMAKE_SOURCE_DIR}/822.cc
	${CMAKE_SOURCE_DIR}/tqueue.cc
	${CMAKE_SOURCE_DIR}/util.cc
	${CMAKE_SOURCE_DIR}/globals.cc)

add_executable(zunpack zunpack.cc ${SOURCES})
target_link_libraries(zunpack archive ${CMAKE_THREAD_LIBS_INIT})

enable_testing()
add_subdirectory(tests)
