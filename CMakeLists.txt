cmake_minimum_required(VERSION 3.3)
project(zdebootstrap CXX)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif (NOT CMAKE_BUILD_TYPE)

set(THREADS_PREFER_PTHREAD_FLAG ON)
include(FindThreads)

set(LIBS archive ${CMAKE_THREAD_LIBS_INIT})

find_library(NUMA numa)
if(NUMA)
	message(STATUS "Found libnuma")
	list(APPEND LIBS numa)
endif()

set(SOURCES
	${CMAKE_SOURCE_DIR}/deb.cc
	${CMAKE_SOURCE_DIR}/822.cc
	${CMAKE_SOURCE_DIR}/tqueue.cc
	${CMAKE_SOURCE_DIR}/util.cc
	${CMAKE_SOURCE_DIR}/nproc.cc
	${CMAKE_SOURCE_DIR}/globals.cc)

add_executable(zunpack zunpack.cc ${SOURCES})
target_link_libraries(zunpack ${LIBS})

enable_testing()
add_subdirectory(tests)
