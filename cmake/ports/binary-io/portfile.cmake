vcpkg_from_github(
	OUT_SOURCE_PATH SOURCE_PATH
	REPO Ryan-rsm-McKenzie/binary_io
	REF 2.0.1
	SHA512 41d416f778f3d42f241611cc999e4f42e227cdcb488d601bbe6e60d85564f70c406c3629437da6cd6e80471b522f0fe56e654f53619e3e5d957498f26472a580
	HEAD_REF main
)

vcpkg_cmake_configure(
	SOURCE_PATH "${SOURCE_PATH}"
	OPTIONS
		-DBUILD_TESTING=OFF
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(
	PACKAGE_NAME binary_io
	CONFIG_PATH "lib/cmake/binary_io"
)

file(REMOVE_RECURSE
	"${CURRENT_PACKAGES_DIR}/debug/include"
	"${CURRENT_PACKAGES_DIR}/debug/share"
)

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
