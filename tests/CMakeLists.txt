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
    target_compile_options(${t} PRIVATE /WX-)
endforeach()

add_executable(opcoda_tests
    pe_parser_test.cpp
    shannon_entropy_test.cpp
    dsp_test.cpp
    rt_test.cpp
    t1_spectral_test.cpp
    end_to_end_test.cpp
    sample_exchange_test.cpp
    note_tracker_test.cpp)

target_link_libraries(opcoda_tests PRIVATE opcoda_core GTest::gtest_main)
add_test(NAME opcoda_tests COMMAND opcoda_tests)

# Processo separado: a substituicao global de operator new vale para o
# executavel inteiro, entao um teste de alocacao em falso so e detectavel se o
# binario for construido em cima da janela de audio.
add_executable(opcoda_alloc_guard_test alloc_guard_test.cpp)
target_link_libraries(opcoda_alloc_guard_test PRIVATE opcoda_core GTest::gtest_main)
add_test(NAME opcoda_alloc_guard COMMAND opcoda_alloc_guard_test)
