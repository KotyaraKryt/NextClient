set(VCPKG_TARGET_ARCHITECTURE x86)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# The build host is x86_64; vcpkg's own community x86-linux triplet does not force
# a 32-bit target, so without this the dependencies would come out 64-bit and fail
# to link against the project's 32-bit objects (GoldSrc is a 32-bit-only engine).
set(VCPKG_C_FLAGS "-m32 -march=pentium4")
set(VCPKG_CXX_FLAGS "-m32 -march=pentium4")
set(VCPKG_LINKER_FLAGS "-m32")
