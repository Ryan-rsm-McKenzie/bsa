set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

find_package(Doxygen MODULE REQUIRED)
if(DOXYGEN_FOUND)
	set(DOXYGEN_BUILTIN_STL_SUPPORT "YES")
	set(DOXYGEN_EXPAND_ONLY_PREDEF "YES")
	set(DOXYGEN_EXTRACT_ALL "YES")
	set(DOXYGEN_MACRO_EXPANSION "YES")
	set(DOXYGEN_PREDEFINED
		"BSA_DOXYGEN"
		"protected=private"
	)
	set(DOXYGEN_STRIP_FROM_INC_PATH "${ROOT_DIR}/include")
	set(DOXYGEN_STRIP_FROM_PATH "${ROOT_DIR}/include")
	set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${ROOT_DIR}/README.md")

	doxygen_add_docs(docs
		${ROOT_DIR}/include
		${ROOT_DIR}/README.md
	)
else()
	message(WARNING "failed to find doxygen: docs will not be built")
endif()
