set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Specify the Clang compilers and LLD linker
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_RC_COMPILER llvm-rc)
set(CMAKE_LINKER lld-link)

# Set the target triple for MSVC compatibility
set(CMAKE_C_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)

# Point to the LLVM MASM-compatible assembler
set(CMAKE_ASM_MASM_COMPILER llvm-ml)

# llvm-ml needs the -m64 flag to act like ml64
set(CMAKE_ASM_MASM_FLAGS "-m64")

# Define the Windows SDK path (adjust the path if necessary)
set(WIN_SDK_PATH "$ENV{HOME}/.xwin-cache/splat")

# Point CMake to the system root, include directories, and library paths
set(CMAKE_SYSROOT ${WIN_SDK_PATH})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Add specific include and library paths for the Windows SDK components
# Use -isystem to treat these as system includes (suppresses warnings)
string(APPEND CMAKE_C_FLAGS " -Xclang -internal-isystem -Xclang ${WIN_SDK_PATH}/crt/include")
string(APPEND CMAKE_C_FLAGS " -Xclang -internal-isystem -Xclang ${WIN_SDK_PATH}/sdk/include/ucrt")
string(APPEND CMAKE_C_FLAGS " -Xclang -internal-isystem -Xclang ${WIN_SDK_PATH}/sdk/include/shared")
string(APPEND CMAKE_C_FLAGS " -Xclang -internal-isystem -Xclang ${WIN_SDK_PATH}/sdk/include/um")
string(APPEND CMAKE_C_FLAGS " -Xclang -internal-isystem -Xclang ${WIN_SDK_PATH}/sdk/include/winrt")

# Same for CXX
string(APPEND CMAKE_CXX_FLAGS " ${CMAKE_C_FLAGS}")

# Ensure the Resource Compiler also knows where to find headers
string(APPEND CMAKE_RC_FLAGS " -I${WIN_SDK_PATH}/crt/include")
string(APPEND CMAKE_RC_FLAGS " -I${WIN_SDK_PATH}/sdk/include/ucrt")
string(APPEND CMAKE_RC_FLAGS " -I${WIN_SDK_PATH}/sdk/include/shared")
string(APPEND CMAKE_RC_FLAGS " -I${WIN_SDK_PATH}/sdk/include/um")

string(APPEND CMAKE_EXE_LINKER_FLAGS " -L${WIN_SDK_PATH}/crt/lib/x86_64")
string(APPEND CMAKE_EXE_LINKER_FLAGS " -L${WIN_SDK_PATH}/sdk/lib/ucrt/x86_64")
string(APPEND CMAKE_EXE_LINKER_FLAGS " -L${WIN_SDK_PATH}/sdk/lib/um/x86_64")
string(APPEND CMAKE_EXE_LINKER_FLAGS " -fuse-ld=lld")

# Apply same flags to shared and module linkers so they find libraries
string(APPEND CMAKE_SHARED_LINKER_FLAGS " ${CMAKE_EXE_LINKER_FLAGS}")
string(APPEND CMAKE_MODULE_LINKER_FLAGS " ${CMAKE_EXE_LINKER_FLAGS}")
