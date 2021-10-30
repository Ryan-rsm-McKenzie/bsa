vcpkg_fail_port_install(ON_TARGET "OSX" "UWP")
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Ryan-rsm-McKenzie/binary_io
    REF 2.0.3
    SHA512 74a7541c5960892df39010ebb2be7711cb26089c34f3ae8c1266065a7d42089929d72d65e23a93db11abbfaef73de56dc770f92d0575c392a73df303248c934d
    HEAD_REF main
)

if(VCPKG_TARGET_IS_LINUX)
    message(WARNING "Build ${PORT} requires at least gcc 10.")
endif()

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
