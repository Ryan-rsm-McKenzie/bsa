vcpkg_from_github(
	OUT_SOURCE_PATH SOURCE_PATH
	REPO Ryan-rsm-McKenzie/binary_io
	REF 1.0.0
	SHA512 1f1429a925ebc0cbc3d2b4a9e512cb173b4b0559d9db51888945ddd1ad1ac713ee670933d8db606c16c3072cb2ad9a251d4ce35fb1027fe43c7be5dfc370ff18
	HEAD_REF main
)

vcpkg_cmake_configure(
	SOURCE_PATH "${SOURCE_PATH}"
	OPTIONS
		-DBUILD_TESTING=OFF
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/binary_io")

file(REMOVE_RECURSE
	"${CURRENT_PACKAGES_DIR}/debug/include"
	"${CURRENT_PACKAGES_DIR}/debug/share"
)

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
