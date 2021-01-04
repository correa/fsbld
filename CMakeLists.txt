cmake_minimum_required(VERSION 3.4.3)

project(fsbld)

include(CheckIncludeFile)

CHECK_INCLUDE_FILE(dirent.h HAS_DIRENT_H)

if (WIN32)
	set(SOURCES windows/fsbld-win.c)
	include_directories(windows)
	if (NOT HAS_DIRENT_H)
		include_directories(windows/dirent/include)
	endif()
else()
	set(SOURCES osx/fsbld.c)
	include_directories(osx)
endif()

add_executable(${PROJECT_NAME} ${SOURCES})