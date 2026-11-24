include(FetchContent)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(gtest_disable_pthreads ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(googletest)

# O GoogleTest 1.15 nao e' limpo sob /WX: gtest-printers.h converte char8_t em
# char32_t implicitamente. A dependencia e' isenta, o codigo do Opcoda nao.
# O /WX- substitui o /WX herdado, por isso o warning D9025: e' esperado.
foreach(t gtest gtest_main gmock gmock_main)
    target_compile_options(${t} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/WX->)
endforeach()

add_executable(opcoda_tests
    pe_parser_test.cpp
    shannon_entropy_test.cpp
    dsp_test.cpp
    rt_test.cpp
    t1_spectral_test.cpp
    end_to_end_test.cpp
    sample_exchange_test.cpp
    byte_range_test.cpp
    byte_position_test.cpp
    column_reduction_test.cpp
    note_tracker_test.cpp
    transport_test.cpp
    transport_rate_test.cpp
    grain_params_test.cpp
    adsr_test.cpp
    filter_test.cpp
    lfo_test.cpp
    character_test.cpp
    mod_matrix_test.cpp
    stereo_width_test.cpp
    output_range_test.cpp
    factory_presets_test.cpp
    grain_trail_test.cpp
    pe_mutation_test.cpp
    disasm_test.cpp)

target_link_libraries(opcoda_tests PRIVATE opcoda_core GTest::gtest_main)
add_test(NAME opcoda_tests COMMAND opcoda_tests)

# Processo separado: a substituicao global de operator new vale para o
# executavel inteiro, entao um teste de alocacao em falso so e detectavel se o
# binario for construido em cima da janela de audio.
add_executable(opcoda_alloc_guard_test alloc_guard_test.cpp)
target_link_libraries(opcoda_alloc_guard_test PRIVATE opcoda_core GTest::gtest_main)
add_test(NAME opcoda_alloc_guard COMMAND opcoda_alloc_guard_test)

# O alloc guard substitui new/delete globais por malloc/free de proposito. O ASan
# do GCC/Clang chama a isso alloc-dealloc-mismatch (a alocacao parece new e a
# libertacao parece free) e aborta antes de o primeiro teste correr; o ASan do
# MSVC nao faz essa verificacao. A recomendacao do proprio ASan para este caso e'
# desligar a verificacao, nao a guarda.
if(OPCODA_ASAN AND NOT MSVC)
    set_tests_properties(opcoda_tests opcoda_alloc_guard PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=alloc_dealloc_mismatch=0")
endif()
