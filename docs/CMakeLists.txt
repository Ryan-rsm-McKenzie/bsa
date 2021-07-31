set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

find_program(POXY_PATH "poxy")

if(POXY_PATH)
	set(DOCS_DIR "${ROOT_DIR}/docs")
	set(INCLUDE_DIR "${ROOT_DIR}/include")

	configure_file(
		"${DOCS_DIR}/poxy.toml.in"
		"${CMAKE_CURRENT_BINARY_DIR}/poxy.toml"
		@ONLY
	)

	add_custom_target(
		docs
		COMMAND
			"${POXY_PATH}"
			"${CMAKE_CURRENT_BINARY_DIR}/poxy.toml"
			"--mcss" "${ROOT_DIR}/external/m.css"
		VERBATIM
		SOURCES
			"${DOCS_DIR}/poxy.toml.in"
	)
else()
	message(WARNING "failed to find poxy: docs will not be built")
endif()
