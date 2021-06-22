if(NOT TARGET common)
	add_library(common INTERFACE)
	add_library(common::common ALIAS common)

	if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
		target_compile_options(
			common
			INTERFACE
				/utf-8	# Set Source and Executable character sets to UTF-8
				/Zc:__cplusplus	# Enable updated __cplusplus macro
				/Zc:preprocessor	# Enable preprocessor conformance mode
		)
	endif()

	target_compile_features(
		common
		INTERFACE
			cxx_std_20
	)

	set_target_properties(
		common
		PROPERTIES
			CXX_STANDARD_REQUIRED ON
	)
endif()
