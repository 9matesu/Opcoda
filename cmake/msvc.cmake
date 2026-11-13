# Toolchain MSVC. O cl.exe precisa das variaveis INCLUDE e LIB do vcvars64, por
# isso o build passa por tools/build.ps1, e nao por `cmake --preset` direto.

# Raizes candidatas, da mais nova para a mais velha. O VS 2026 (BuildTools 18)
# traz o toolset v145; o VS 2022 continua como alternativa.
set(OPCODA_VS_ROOT "" CACHE PATH "Visual Studio Build Tools (vazio: autodetecta)")
set(OPCODA_MSVC_VERSION "" CACHE STRING "Versao do toolset MSVC (vazio: a mais nova instalada)")

if(NOT OPCODA_VS_ROOT)
    set(_opcoda_vs_candidates
        "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools"
        "C:/Program Files/Microsoft Visual Studio/18/BuildTools"
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools"
        "C:/Program Files/Microsoft Visual Studio/2022/BuildTools"
        "C:/Program Files/Microsoft Visual Studio/2022/Professional"
        "C:/Program Files/Microsoft Visual Studio/2022/Community"
        "C:/Program Files/Microsoft Visual Studio/2022/Enterprise"
    )
    foreach(_candidate IN LISTS _opcoda_vs_candidates)
        if(NOT OPCODA_VS_ROOT AND EXISTS "${_candidate}/VC/Tools/MSVC")
            set(OPCODA_VS_ROOT "${_candidate}")
        endif()
    endforeach()
    if(NOT OPCODA_VS_ROOT)
        message(FATAL_ERROR "Visual Studio Build Tools nao encontrado; passe -DOPCODA_VS_ROOT=")
    endif()
    message(STATUS "MSVC: raiz ${OPCODA_VS_ROOT}")
endif()

if(NOT OPCODA_MSVC_VERSION)
    file(GLOB _opcoda_toolsets LIST_DIRECTORIES true "${OPCODA_VS_ROOT}/VC/Tools/MSVC/*")
    list(SORT _opcoda_toolsets COMPARE NATURAL ORDER DESCENDING)
    foreach(_toolset IN LISTS _opcoda_toolsets)
        if(NOT OPCODA_MSVC_VERSION AND EXISTS "${_toolset}/bin/Hostx64/x64/cl.exe")
            get_filename_component(OPCODA_MSVC_VERSION "${_toolset}" NAME)
        endif()
    endforeach()
    if(NOT OPCODA_MSVC_VERSION)
        message(FATAL_ERROR "toolset MSVC nao encontrado em ${OPCODA_VS_ROOT}/VC/Tools/MSVC")
    endif()
    message(STATUS "MSVC: toolset ${OPCODA_MSVC_VERSION}")
endif()

get_filename_component(OPCODA_MSVC_BIN "${OPCODA_VS_ROOT}/VC/Tools/MSVC/${OPCODA_MSVC_VERSION}/bin/Hostx64/x64" ABSOLUTE)
if(NOT EXISTS "${OPCODA_MSVC_BIN}/cl.exe")
    message(FATAL_ERROR "cl.exe nao encontrado em ${OPCODA_MSVC_BIN}")
endif()

set(CMAKE_C_COMPILER   "${OPCODA_MSVC_BIN}/cl.exe")
set(CMAKE_CXX_COMPILER "${OPCODA_MSVC_BIN}/cl.exe")
set(CMAKE_LINKER "${OPCODA_MSVC_BIN}/link.exe")
