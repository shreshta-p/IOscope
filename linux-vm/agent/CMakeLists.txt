cmake_minimum_required(VERSION 3.20)
project(IOscopeNativeLinux VERSION 0.1.0 LANGUAGES C CXX)

if(WIN32)
  message(FATAL_ERROR "This is the Linux agent build; use windows/agent for Windows/MSVC")
endif()

include(FetchContent)
FetchContent_Declare(crow
  URL https://codeload.github.com/CrowCpp/Crow/zip/refs/tags/v1.2.1.2
  URL_HASH SHA256=eb52839043358830e09976198df5c1e8855a75730ccd3f1d8799eff0a79609b1)
FetchContent_Declare(asio
  URL https://codeload.github.com/chriskohlhoff/asio/zip/refs/tags/asio-1-30-2
  URL_HASH SHA256=948373ed2e838ee1181937380c754e8fe6da8b18022eae922bcdc0cd38d6dbde)
FetchContent_Declare(json
  URL https://codeload.github.com/nlohmann/json/zip/refs/tags/v3.12.0
  URL_HASH SHA256=34660b5e9a407195d55e8da705ed26cc6d175ce5a6b1fb957e701fb4d5b04022)
FetchContent_Declare(sqlite
  URL https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip
  URL_HASH SHA256=1e71ddf93849c6a6ecf58b827c0692073d2dd7ee40196158068f7b29f422e87d)
FetchContent_Declare(jsoncons
  URL https://codeload.github.com/danielaparker/jsoncons/zip/refs/tags/v1.7.0
  URL_HASH SHA256=63912bce2dbfd40d8cb1920adeaa4c1f94302fec2969f16d54ee6c9a8808c42c)

# Populate source only: no dependency examples, tests, installation or global flags.
# Same set as windows/agent/CMakeLists.txt (baseline commit 280ced5) — these libraries
# are cross-platform; only the compiler/link flags below differ from the MSVC build.
foreach(dependency crow asio json sqlite jsoncons)
  FetchContent_GetProperties(${dependency})
  if(NOT ${dependency}_POPULATED)
    FetchContent_Populate(${dependency})
  endif()
endforeach()

# Bound HTTP bodies during parsing, before middleware sees the request. Crow 1.2
# exposes a WebSocket limit but no corresponding HTTP-body setting. Same patch as
# the Windows build; Crow's parser.h is platform-independent.
file(READ "${crow_SOURCE_DIR}/include/crow/parser.h" CROW_PARSER_SOURCE)
string(REPLACE "self->req.body.insert(self->req.body.end(), at, at + length);"
  "if (length > 32 * 1024 * 1024 || self->req.body.size() > 32 * 1024 * 1024 - length) return 1; self->req.body.insert(self->req.body.end(), at, at + length);"
  CROW_PARSER_BOUNDED "${CROW_PARSER_SOURCE}")
if(NOT CROW_PARSER_SOURCE MATCHES "length > 32 \\* 1024")
  file(WRITE "${crow_SOURCE_DIR}/include/crow/parser.h" "${CROW_PARSER_BOUNDED}")
endif()

add_library(ioscope_sqlite STATIC "${sqlite_SOURCE_DIR}/sqlite3.c")
target_include_directories(ioscope_sqlite SYSTEM PUBLIC "${sqlite_SOURCE_DIR}")
target_compile_definitions(ioscope_sqlite PRIVATE SQLITE_THREADSAFE=1 SQLITE_OMIT_LOAD_EXTENSION)

add_library(ioscope_native_dependencies INTERFACE)
target_include_directories(ioscope_native_dependencies SYSTEM INTERFACE
  "${crow_SOURCE_DIR}/include" "${asio_SOURCE_DIR}/asio/include"
  "${json_SOURCE_DIR}/single_include" "${jsoncons_SOURCE_DIR}/include")
target_compile_definitions(ioscope_native_dependencies INTERFACE ASIO_STANDALONE)
target_link_libraries(ioscope_native_dependencies INTERFACE ioscope_sqlite pthread dl)

# -Wno-misleading-indentation: this codebase (including code ported verbatim from
# windows/agent) uses a dense "if(x)stmt;stmt2;" style throughout that MSVC's /W4
# doesn't flag; GCC's stricter check would otherwise force reformatting files that
# are meant to stay diffable against the Windows baseline.
# -Wno-format-truncation: a known GCC false positive on snprintf with struct tm
# fields (utc_now() in platform.hpp) — GCC's static range analysis can't see that
# tm_year/tm_mon/.../tv_nsec are bounded to small values, so it assumes worst-case
# int range and overestimates the possible output width. The values are genuinely
# bounded (a valid calendar date, 0-999ms); the fixed 32-byte buffer is correct.
set(IOSCOPE_WARNINGS -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-format-truncation)

add_executable(ioscope_native_preflight preflight.cpp)
target_compile_features(ioscope_native_preflight PRIVATE cxx_std_20)
target_compile_options(ioscope_native_preflight PRIVATE ${IOSCOPE_WARNINGS})
target_link_libraries(ioscope_native_preflight PRIVATE ioscope_native_dependencies)

enable_testing()
add_test(NAME native_preflight COMMAND ioscope_native_preflight)
set_tests_properties(native_preflight PROPERTIES TIMEOUT 10)

add_executable(ioscope_store_tests store_tests.cpp)
target_compile_features(ioscope_store_tests PRIVATE cxx_std_20)
target_link_libraries(ioscope_store_tests PRIVATE ioscope_native_dependencies)
target_compile_options(ioscope_store_tests PRIVATE ${IOSCOPE_WARNINGS})
add_test(NAME store_tests COMMAND ioscope_store_tests)

add_executable(ioscope_contract_tests contract_tests.cpp)
target_compile_features(ioscope_contract_tests PRIVATE cxx_std_20)
target_compile_options(ioscope_contract_tests PRIVATE ${IOSCOPE_WARNINGS})
target_link_libraries(ioscope_contract_tests PRIVATE ioscope_native_dependencies)
add_test(NAME contract_tests COMMAND ioscope_contract_tests "${CMAKE_CURRENT_SOURCE_DIR}/../contracts")

add_executable(ioscope_agent main.cpp)
target_compile_features(ioscope_agent PRIVATE cxx_std_20)
target_compile_options(ioscope_agent PRIVATE ${IOSCOPE_WARNINGS})
target_link_libraries(ioscope_agent PRIVATE ioscope_native_dependencies)

foreach(target ioscope_store_tests ioscope_contract_tests ioscope_agent ioscope_native_preflight)
  target_compile_options(${target} PRIVATE
    "-isystem${crow_SOURCE_DIR}/include" "-isystem${asio_SOURCE_DIR}/asio/include"
    "-isystem${json_SOURCE_DIR}/single_include" "-isystem${jsoncons_SOURCE_DIR}/include"
    "-isystem${sqlite_SOURCE_DIR}")
endforeach()

add_executable(ioscope_json_input_tests json_input_tests.cpp)
target_compile_features(ioscope_json_input_tests PRIVATE cxx_std_20)
target_compile_options(ioscope_json_input_tests PRIVATE ${IOSCOPE_WARNINGS} "-isystem${json_SOURCE_DIR}/single_include")
target_link_libraries(ioscope_json_input_tests PRIVATE ioscope_native_dependencies)
add_test(NAME json_input_tests COMMAND ioscope_json_input_tests)

add_executable(ioscope_counter_math_tests counter_math_tests.cpp)
target_compile_features(ioscope_counter_math_tests PRIVATE cxx_std_20)
target_compile_options(ioscope_counter_math_tests PRIVATE ${IOSCOPE_WARNINGS})
add_test(NAME counter_math_tests COMMAND ioscope_counter_math_tests)

add_executable(ioscope_safety_tests safety_tests.cpp)
target_compile_features(ioscope_safety_tests PRIVATE cxx_std_20)
target_compile_options(ioscope_safety_tests PRIVATE ${IOSCOPE_WARNINGS} "-isystem${json_SOURCE_DIR}/single_include")
target_link_libraries(ioscope_safety_tests PRIVATE ioscope_native_dependencies)
add_test(NAME safety_tests COMMAND ioscope_safety_tests "${CMAKE_CURRENT_SOURCE_DIR}/../contracts/fixtures/WorkloadDefinition.json")

add_executable(ioscope_process_job_tests process_job_tests.cpp)
target_compile_features(ioscope_process_job_tests PRIVATE cxx_std_20)
target_compile_options(ioscope_process_job_tests PRIVATE ${IOSCOPE_WARNINGS} "-isystem${json_SOURCE_DIR}/single_include")
target_link_libraries(ioscope_process_job_tests PRIVATE ioscope_native_dependencies)
add_test(NAME process_job_tests COMMAND ioscope_process_job_tests)
set_tests_properties(process_job_tests PROPERTIES TIMEOUT 30)

add_executable(ioscope_scratch_tests scratch_tests.cpp)
target_compile_features(ioscope_scratch_tests PRIVATE cxx_std_20)
target_compile_options(ioscope_scratch_tests PRIVATE ${IOSCOPE_WARNINGS} "-isystem${json_SOURCE_DIR}/single_include")
target_link_libraries(ioscope_scratch_tests PRIVATE ioscope_native_dependencies)
add_test(NAME scratch_tests COMMAND ioscope_scratch_tests)
add_dependencies(ioscope_scratch_tests ioscope_prepare)

add_executable(ioscope_fio_tests fio_tests.cpp)
target_compile_features(ioscope_fio_tests PRIVATE cxx_std_20)
target_compile_options(ioscope_fio_tests PRIVATE ${IOSCOPE_WARNINGS} "-isystem${json_SOURCE_DIR}/single_include")
target_link_libraries(ioscope_fio_tests PRIVATE ioscope_native_dependencies)
add_test(NAME fio_tests COMMAND ioscope_fio_tests "${CMAKE_CURRENT_SOURCE_DIR}/../contracts/fixtures/WorkloadDefinition.json")

add_executable(ioscope_workload_controller_tests workload_controller_tests.cpp)
target_compile_features(ioscope_workload_controller_tests PRIVATE cxx_std_20)
target_compile_options(ioscope_workload_controller_tests PRIVATE ${IOSCOPE_WARNINGS} "-isystem${json_SOURCE_DIR}/single_include" "-isystem${jsoncons_SOURCE_DIR}/include")
target_link_libraries(ioscope_workload_controller_tests PRIVATE ioscope_native_dependencies)
add_test(NAME workload_controller_tests COMMAND ioscope_workload_controller_tests "${CMAKE_CURRENT_SOURCE_DIR}/../contracts" "${CMAKE_CURRENT_BINARY_DIR}/native-test-recordings.json")
set_tests_properties(workload_controller_tests PROPERTIES TIMEOUT 15)

add_executable(ioscope_experiment_controller_tests experiment_controller_tests.cpp)
target_compile_features(ioscope_experiment_controller_tests PRIVATE cxx_std_20)
target_compile_options(ioscope_experiment_controller_tests PRIVATE ${IOSCOPE_WARNINGS} "-isystem${json_SOURCE_DIR}/single_include" "-isystem${jsoncons_SOURCE_DIR}/include")
target_link_libraries(ioscope_experiment_controller_tests PRIVATE ioscope_native_dependencies)
add_test(NAME experiment_controller_tests COMMAND ioscope_experiment_controller_tests "${CMAKE_CURRENT_SOURCE_DIR}/../contracts")
set_tests_properties(experiment_controller_tests PROPERTIES TIMEOUT 30)

add_executable(ioscope_analyzer_tests analyzer_tests.cpp)
target_compile_features(ioscope_analyzer_tests PRIVATE cxx_std_20)
target_compile_options(ioscope_analyzer_tests PRIVATE ${IOSCOPE_WARNINGS} "-isystem${json_SOURCE_DIR}/single_include" "-isystem${jsoncons_SOURCE_DIR}/include")
target_link_libraries(ioscope_analyzer_tests PRIVATE ioscope_native_dependencies)
add_test(NAME analyzer_tests COMMAND ioscope_analyzer_tests)

add_executable(ioscope_prepare prepare.cpp)
target_compile_features(ioscope_prepare PRIVATE cxx_std_20)
target_compile_options(ioscope_prepare PRIVATE ${IOSCOPE_WARNINGS} "-isystem${json_SOURCE_DIR}/single_include")
target_link_libraries(ioscope_prepare PRIVATE ioscope_native_dependencies)
