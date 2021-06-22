cmake_minimum_required(VERSION 3.20)

project(text LANGUAGES CXX)

include(GNUInstallDirs)

file(GLOB SOURCES src/*.cpp)

if(DEFINED Boost_USE_MULTITHREADED AND NOT Boost_USE_MULTITHREADED)
	set(MULTITHREADED_TAG "st")
else()
	set(MULTITHREADED_TAG "mt")
endif()

if(DEFINED Boost_USE_DEBUG_RUNTIME AND NOT Boost_USE_DEBUG_RUNTIME)
	set(USE_DEBUG_RUNTIME_TAG "")
else()
	set(USE_DEBUG_RUNTIME_TAG "g")
endif()

add_library(${PROJECT_NAME} ${SOURCES})
add_library(Boost::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

if(MSVC)
	set_target_properties(
		${PROJECT_NAME}
		PROPERTIES
			DEBUG_POSTFIX "-${USE_DEBUG_RUNTIME_TAG}d"
			OUTPUT_NAME "boost_${PROJECT_NAME}-vc${MSVC_TOOLSET_VERSION}-${MULTITHREADED_TAG}"
	)

	target_compile_options(
		${PROJECT_NAME}
		PRIVATE
			/utf-8
			/W0
	)
endif()

target_compile_definitions(
	${PROJECT_NAME}
	PUBLIC
		BOOST_TEXT_DISABLE_CONCEPTS	# totally broken
)

target_include_directories(
	${PROJECT_NAME}
	PUBLIC
		"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
		"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

find_package(
	Boost
	MODULE
	REQUIRED
	COMPONENTS
		container
		filesystem
)

target_link_libraries(
	${PROJECT_NAME}
	PUBLIC
		Boost::container
		Boost::filesystem
		Boost::headers
)

install(
	TARGETS ${PROJECT_NAME}
	EXPORT ${PROJECT_NAME}-targets
)

configure_file(
	config.cmake.in
	${PROJECT_NAME}Config.cmake
	@ONLY
)

install(
	FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

install(
	EXPORT ${PROJECT_NAME}-targets
	NAMESPACE Boost::
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

install(
	DIRECTORY "include/boost/${PROJECT_NAME}"
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/boost"
)
