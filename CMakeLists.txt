cmake_minimum_required(VERSION 3.20)
project(toraIME LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TORA_BUILD_TESTS "エンジンのテストをビルドする" ON)
option(TORA_BUILD_TSF "Windows 用 IME (TSF) の DLL をビルドする" ${WIN32})

if(MSVC)
  add_compile_options(/utf-8 /W4 /permissive- /EHsc)
  add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  # IME はあらゆるプロセスに読み込まれるので CRT を静的リンクする
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
  add_compile_options(-Wall -Wextra)
  if(WIN32)
    add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  endif()
endif()

# --- 変換エンジン (OS 非依存) ---
add_library(tora_engine STATIC
  src/engine/composer.cpp
  src/engine/converter.cpp
  src/engine/dictionary.cpp
  src/engine/engine.cpp
  src/engine/input.cpp
  src/engine/mapped_file.cpp
  src/engine/romaji.cpp
  src/engine/session.cpp
  src/engine/text_util.cpp
  src/engine/user_data.cpp
)
target_include_directories(tora_engine PUBLIC src/engine)
if(WIN32)
  set_property(TARGET tora_engine PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

# --- テスト ---
if(TORA_BUILD_TESTS)
  enable_testing()
  add_executable(engine_tests tests/engine_tests.cpp)
  target_link_libraries(engine_tests PRIVATE tora_engine)
  target_compile_definitions(engine_tests PRIVATE
    TORA_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME engine_tests COMMAND engine_tests)
  # 本物の辞書を使うテスト (TORA_DIC に toraime.dic のパスを指定したときだけ)
  if(DEFINED TORA_DIC)
    add_test(NAME engine_tests_with_dictionary COMMAND engine_tests --dic ${TORA_DIC})
  endif()
endif()

# --- Windows IME (TSF テキストサービス) ---
if(TORA_BUILD_TSF)
  add_library(toraime SHARED
    src/tsf/candidate_window.cpp
    src/tsf/class_factory.cpp
    src/tsf/compartment.cpp
    src/tsf/display_attribute.cpp
    src/tsf/dllmain.cpp
    src/tsf/edit_session.cpp
    src/tsf/key_translator.cpp
    src/tsf/language_bar.cpp
    src/tsf/registry.cpp
    src/tsf/settings.cpp
    src/tsf/text_service.cpp
    src/tsf/toraime.def
    src/tsf/toraime.rc
  )
  target_link_libraries(toraime PRIVATE tora_engine ole32 oleaut32 uuid advapi32 user32 gdi32 shell32 shlwapi)
  if(NOT MSVC)
    target_link_options(toraime PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
  set_target_properties(toraime PROPERTIES PREFIX "")
endif()
