if(NOT TARGET bsa_common)
	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set(CMAKE_CXX_EXTENSIONS OFF)
	endif()

	add_library(bsa_common INTERFACE)
	add_library(bsa::common ALIAS bsa_common)

	if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
		target_compile_options(
			bsa_common
			INTERFACE
				/utf-8	# Set Source and Executable character sets to UTF-8
				/Zc:__cplusplus	# Enable updated __cplusplus macro
				/Zc:preprocessor	# Enable preprocessor conformance mode
		)
	endif()

	target_compile_features(
		bsa_common
		INTERFACE
			cxx_std_20
	)

	set_target_properties(
		bsa_common
		PROPERTIES
			CXX_STANDARD_REQUIRED ON
	)

	install(
		TARGETS bsa_common
		EXPORT bsa_common-targets
	)

	install(
		EXPORT bsa_common-targets
		NAMESPACE bsa::
		DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bsa
	)
endif()
