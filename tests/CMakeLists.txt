set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(SOURCE_DIR ${ROOT_DIR}/tests)
set(SOURCE_FILES
	${SOURCE_DIR}/main.cpp
	${SOURCE_DIR}/tes4_tests.cpp
)
source_group("src" FILES ${SOURCE_FILES})

set(NATVIS_FILES
	${ROOT_DIR}/extern/CPPDebuggerVisualizers/VS2019/Visualizers/boost_Containers.natvis
)
source_group("visualizers" FILES ${NATVIS_FILES})

add_executable(
	tests
	${SOURCE_FILES}
	${NATVIS_FILES}
)
catch_discover_tests(tests)

target_link_libraries(
	tests
	PUBLIC
		bsa::bsa
		Catch2::Catch2
		common::common
)
