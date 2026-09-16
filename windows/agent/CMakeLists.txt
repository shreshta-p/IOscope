cmake_minimum_required(VERSION 3.20)
project(IOscopeNativePreflight VERSION 0.1.0 LANGUAGES C CXX)

if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "This preflight validates Windows with MSVC only")
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
foreach(dependency crow asio json sqlite jsoncons)
  FetchContent_GetProperties(${dependency})
  if(NOT ${dependency}_POPULATED)
    FetchContent_Populate(${dependency})
  endif()
endforeach()

# Bound HTTP bodies during parsing, before middleware sees the request. Crow 1.2
# exposes a WebSocket limit but no corresponding HTTP-body setting.
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
target_compile_definitions(ioscope_native_dependencies INTERFACE
  ASIO_STANDALONE _WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(ioscope_native_dependencies INTERFACE ioscope_sqlite ws2_32 mswsock)

add_executable(ioscope_native_preflight preflight.cpp)
target_compile_features(ioscope_native_preflight PRIVATE cxx_std_20)
target_compile_options(ioscope_native_preflight PRIVATE
  /W4 /WX /permissive- /EHsc /utf-8 /Zc:__cplusplus /external:W0
  "/external:I${crow_SOURCE_DIR}/include"
  "/external:I${asio_SOURCE_DIR}/asio/include"
  "/external:I${json_SOURCE_DIR}/single_include"
  "/external:I${sqlite_SOURCE_DIR}")
target_link_libraries(ioscope_native_preflight PRIVATE ioscope_native_dependencies)

enable_testing()
add_test(NAME native_preflight COMMAND ioscope_native_preflight)
set_tests_properties(native_preflight PROPERTIES TIMEOUT 10)

add_executable(ioscope_store_tests store_tests.cpp)
target_compile_features(ioscope_store_tests PRIVATE cxx_std_20)
target_link_libraries(ioscope_store_tests PRIVATE ioscope_native_dependencies)
target_compile_options(ioscope_store_tests PRIVATE /W4 /EHsc /utf-8 /Zc:__cplusplus /bigobj)
add_test(NAME store_tests COMMAND ioscope_store_tests)
add_executable(ioscope_contract_tests contract_tests.cpp)
target_compile_features(ioscope_contract_tests PRIVATE cxx_std_20)
target_compile_options(ioscope_contract_tests PRIVATE /W4 /EHsc /utf-8 /Zc:__cplusplus /bigobj)
target_link_libraries(ioscope_contract_tests PRIVATE ioscope_native_dependencies bcrypt shell32 ole32)
add_test(NAME contract_tests COMMAND ioscope_contract_tests "${CMAKE_CURRENT_SOURCE_DIR}/../contracts")
add_executable(ioscope_agent main.cpp)
target_compile_features(ioscope_agent PRIVATE cxx_std_20)
target_compile_options(ioscope_agent PRIVATE /W4 /EHsc /utf-8 /Zc:__cplusplus /bigobj)
target_link_libraries(ioscope_agent PRIVATE ioscope_native_dependencies bcrypt shell32 ole32 pdh advapi32 xmllite shlwapi)

foreach(target ioscope_store_tests ioscope_contract_tests ioscope_agent)
  target_compile_options(${target} PRIVATE /external:W0
    "/external:I${crow_SOURCE_DIR}/include"
    "/external:I${asio_SOURCE_DIR}/asio/include"
    "/external:I${json_SOURCE_DIR}/single_include"
    "/external:I${jsoncons_SOURCE_DIR}/include"
    "/external:I${sqlite_SOURCE_DIR}")
endforeach()
add_executable(ioscope_json_input_tests json_input_tests.cpp)
target_compile_features(ioscope_json_input_tests PRIVATE cxx_std_20)
target_link_libraries(ioscope_json_input_tests PRIVATE ioscope_native_dependencies)
add_test(NAME json_input_tests COMMAND ioscope_json_input_tests)

add_executable(ioscope_counter_math_tests counter_math_tests.cpp)
target_compile_features(ioscope_counter_math_tests PRIVATE cxx_std_20)
add_test(NAME counter_math_tests COMMAND ioscope_counter_math_tests)
add_executable(ioscope_safety_tests safety_tests.cpp)
target_compile_features(ioscope_safety_tests PRIVATE cxx_std_20)
target_link_libraries(ioscope_safety_tests PRIVATE ioscope_native_dependencies)
add_test(NAME safety_tests COMMAND ioscope_safety_tests "${CMAKE_CURRENT_SOURCE_DIR}/../contracts/fixtures/WorkloadDefinition.json")
add_executable(ioscope_process_job_tests process_job_tests.cpp)
target_compile_features(ioscope_process_job_tests PRIVATE cxx_std_20)
target_link_libraries(ioscope_process_job_tests PRIVATE ioscope_native_dependencies)
add_test(NAME process_job_tests COMMAND ioscope_process_job_tests)
set_tests_properties(process_job_tests PROPERTIES TIMEOUT 15)
add_executable(ioscope_scratch_tests scratch_tests.cpp)
target_compile_features(ioscope_scratch_tests PRIVATE cxx_std_20)
target_link_libraries(ioscope_scratch_tests PRIVATE ioscope_native_dependencies bcrypt shell32 ole32 advapi32)
add_test(NAME scratch_tests COMMAND ioscope_scratch_tests)
add_executable(ioscope_diskspd_tests diskspd_tests.cpp)
target_compile_features(ioscope_diskspd_tests PRIVATE cxx_std_20)
target_link_libraries(ioscope_diskspd_tests PRIVATE ioscope_native_dependencies bcrypt xmllite shlwapi ole32)
add_test(NAME diskspd_tests COMMAND ioscope_diskspd_tests "${CMAKE_CURRENT_SOURCE_DIR}/../contracts/fixtures/WorkloadDefinition.json")
add_executable(ioscope_workload_controller_tests workload_controller_tests.cpp)
target_compile_features(ioscope_workload_controller_tests PRIVATE cxx_std_20)
target_compile_options(ioscope_workload_controller_tests PRIVATE /bigobj /EHsc /utf-8)
target_link_libraries(ioscope_workload_controller_tests PRIVATE ioscope_native_dependencies bcrypt shell32 ole32 advapi32 xmllite shlwapi)
add_test(NAME workload_controller_tests COMMAND ioscope_workload_controller_tests "${CMAKE_CURRENT_SOURCE_DIR}/../contracts" "${CMAKE_CURRENT_BINARY_DIR}/native-test-recordings.json")
set_tests_properties(workload_controller_tests PROPERTIES TIMEOUT 15)
add_executable(ioscope_prepare prepare.cpp)
target_compile_features(ioscope_prepare PRIVATE cxx_std_20)
target_link_libraries(ioscope_prepare PRIVATE ioscope_native_dependencies bcrypt shell32 ole32 advapi32)
