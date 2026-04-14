set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Override this from CLion/CMake profile if your path differs.
if(NOT DEFINED ARM_GCC_BIN)
    set(ARM_GCC_BIN "D:/ProgrammingTools/STM32CubeCLT_1.21.0/GNU-tools-for-STM32/bin")
endif()

set(TOOLCHAIN_PREFIX arm-none-eabi)

set(CMAKE_C_COMPILER   "${ARM_GCC_BIN}/${TOOLCHAIN_PREFIX}-gcc.exe")
set(CMAKE_CXX_COMPILER "${ARM_GCC_BIN}/${TOOLCHAIN_PREFIX}-g++.exe")
set(CMAKE_ASM_COMPILER "${ARM_GCC_BIN}/${TOOLCHAIN_PREFIX}-gcc.exe")
set(CMAKE_AR           "${ARM_GCC_BIN}/${TOOLCHAIN_PREFIX}-ar.exe")
set(CMAKE_OBJCOPY      "${ARM_GCC_BIN}/${TOOLCHAIN_PREFIX}-objcopy.exe")
set(CMAKE_OBJDUMP      "${ARM_GCC_BIN}/${TOOLCHAIN_PREFIX}-objdump.exe")
set(CMAKE_SIZE         "${ARM_GCC_BIN}/${TOOLCHAIN_PREFIX}-size.exe")
set(CMAKE_GDB          "${ARM_GCC_BIN}/${TOOLCHAIN_PREFIX}-gdb.exe")
