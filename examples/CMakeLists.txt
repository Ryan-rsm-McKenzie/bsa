add_library(examples INTERFACE)
add_library(bsa::examples ALIAS examples)

macro(add_example NAME)
	add_executable("example_${NAME}" "${NAME}.cpp")
	source_group("src" FILES "${NAME}.cpp")

	set_target_properties(
		"example_${NAME}"
		PROPERTIES
			OUTPUT_NAME ${NAME}
	)

	target_link_libraries(
		"example_${NAME}"
		PRIVATE
			bsa::bsa
			bsa::examples
	)
endmacro()

add_example(pack_unpack)
