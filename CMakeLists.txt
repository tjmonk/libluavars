cmake_minimum_required(VERSION 3.10)

project(luavars
	VERSION 0.1
	DESCRIPTION "LUA Varserver Interface"
)

include(GNUInstallDirs)

add_library( ${PROJECT_NAME} SHARED
	src/libluavars.c
)

target_link_libraries( ${PROJECT_NAME}
	dl
	rt
	varserver
)

set_target_properties( ${PROJECT_NAME} PROPERTIES
	VERSION ${PROJECT_VERSION}
	SOVERSION 1
	POSITION_INDEPENDENT_CODE ON
)

install(TARGETS ${PROJECT_NAME}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

