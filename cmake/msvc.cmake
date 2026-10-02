# Toolchain MSVC. O cl.exe precisa das variaveis INCLUDE e LIB do vcvars64, por
# isso o build passa por tools/build.ps1, e nao por `cmake --preset` direto.

set(OPCODA_VS_ROOT "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools" CACHE PATH "Visual Studio Build Tools")
set(OPCODA_MSVC_VERSION "14.51.36231" CACHE STRING "Versao do toolset MSVC")

get_filename_component(OPCODA_MSVC_BIN "${OPCODA_VS_ROOT}/VC/Tools/MSVC/${OPCODA_MSVC_VERSION}/bin/Hostx64/x64" ABSOLUTE)
if(NOT EXISTS "${OPCODA_MSVC_BIN}/cl.exe")
    message(FATAL_ERROR "cl.exe nao encontrado em ${OPCODA_MSVC_BIN}")
endif()

set(CMAKE_C_COMPILER   "${OPCODA_MSVC_BIN}/cl.exe")
set(CMAKE_CXX_COMPILER "${OPCODA_MSVC_BIN}/cl.exe")
set(CMAKE_LINKER "${OPCODA_MSVC_BIN}/link.exe")
