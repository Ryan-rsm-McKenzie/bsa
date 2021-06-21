vcpkg_from_github(
	OUT_SOURCE_PATH SOURCE_PATH
	REPO Ryan-rsm-McKenzie/text
	REF 15aaea4297e00ec4c74295e7913ead79c90e1502
	SHA512 980be6d280b8e04898c458312148e32ddc64829478527e04d7bf25ee16412a8098b1a614156dc8cd9d2276d02908b92160be68a63ae24c13db6f33500a84fe9e
	HEAD_REF master
)

file(
	COPY
		${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt
		${CMAKE_CURRENT_LIST_DIR}/config.cmake.in
	DESTINATION
		${SOURCE_PATH}
)

vcpkg_cmake_configure(SOURCE_PATH ${SOURCE_PATH})
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(
	PACKAGE_NAME text
	CONFIG_PATH lib/cmake/text
)

file(
	REMOVE_RECURSE
		${CURRENT_PACKAGES_DIR}/debug/include
		${CURRENT_PACKAGES_DIR}/debug/share
)

file(INSTALL ${SOURCE_PATH}/LICENSE_1_0.txt DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
