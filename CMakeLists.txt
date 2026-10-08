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

# --- UI の描画 (グラスモーフィズム。OS 非依存の部分 + 文字の描画) ---
set(TORA_UI_SOURCES src/ui/canvas.cpp src/ui/candidate_view.cpp src/ui/settings_view.cpp)
if(WIN32)
  add_library(tora_ui STATIC ${TORA_UI_SOURCES} src/ui/font_win.cpp)
  target_link_libraries(tora_ui PUBLIC gdi32 user32 advapi32)
  set(TORA_HAVE_UI ON)
else()
  find_package(Freetype QUIET)
  if(FREETYPE_FOUND)
    add_library(tora_ui STATIC ${TORA_UI_SOURCES} src/ui/font_ft.cpp)
    target_link_libraries(tora_ui PUBLIC Freetype::Freetype)
    set(TORA_HAVE_UI ON)
    # デザイン確認用: ui_preview <フォント> <アイコンの生 ARGB 128x128> <出力ディレクトリ>
    add_executable(ui_preview tools/ui_preview.cpp)
    target_link_libraries(ui_preview PRIVATE tora_ui)
  endif()
endif()
if(TORA_HAVE_UI)
  target_include_directories(tora_ui PUBLIC src/ui src/engine)
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
    src/tsf/guids.cpp
    src/tsf/key_translator.cpp
    src/tsf/language_bar.cpp
    src/tsf/registry.cpp
    src/tsf/settings.cpp
    src/tsf/text_service.cpp
    src/tsf/toraime.def
    src/tsf/toraime.rc
  )
  target_link_libraries(toraime PRIVATE tora_engine tora_ui ole32 oleaut32 uuid advapi32 user32 gdi32 shell32 shlwapi)
  target_include_directories(toraime PRIVATE src/tsf)
  target_compile_definitions(toraime PRIVATE _WIN32_WINNT=0x0A00)
  if(NOT MSVC)
    target_link_options(toraime PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
  set_target_properties(toraime PROPERTIES PREFIX "")

  # 設定アプリ
  add_executable(toraime_settings WIN32
    src/settings_app/main.cpp
    src/settings_app/settings_app.rc
    src/tsf/settings.cpp
  )
  target_include_directories(toraime_settings PRIVATE src/settings_app)
  target_compile_definitions(toraime_settings PRIVATE _WIN32_WINNT=0x0A00)
  target_link_libraries(toraime_settings PRIVATE tora_engine tora_ui dwmapi ole32 uuid shell32 advapi32 user32 gdi32)
  if(MSVC)
    # マニフェストは .rc で埋め込む
    target_link_options(toraime_settings PRIVATE /MANIFEST:NO)
  else()
    target_link_options(toraime_settings PRIVATE -municode -static -static-libgcc -static-libstdc++)
  endif()

  # 自前の GUID 定義が SDK と一致するかの確認 (SDK の定義がある MSVC のみ)
  if(MSVC AND TORA_BUILD_TESTS)
    add_executable(tsf_guid_check tests/tsf_guid_check.cpp src/tsf/guids.cpp)
    target_link_libraries(tsf_guid_check PRIVATE uuid ole32)
    add_test(NAME tsf_guid_check COMMAND tsf_guid_check)

    # TSF のスレッドマネージャーで有効化・無効化してみる
    get_target_property(TORA_TSF_SOURCES toraime SOURCES)
    list(FILTER TORA_TSF_SOURCES INCLUDE REGEX "\\.cpp$")
    add_executable(tsf_smoke tests/tsf_smoke.cpp ${TORA_TSF_SOURCES})
    target_compile_definitions(tsf_smoke PRIVATE _WIN32_WINNT=0x0A00)
    target_link_libraries(tsf_smoke PRIVATE tora_engine tora_ui uuid ole32 oleaut32 advapi32 user32 gdi32 shell32 shlwapi)
    add_test(NAME tsf_smoke COMMAND tsf_smoke)
  endif()
endif()

# --- 辞書の生成 (cmake --build . --target dictionary) ---
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  add_custom_target(dictionary
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tools/fetch_mozc.py ${CMAKE_BINARY_DIR}/mozc
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_dict.py ${CMAKE_BINARY_DIR}/mozc
            ${CMAKE_BINARY_DIR}/toraime.dic
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_english.py ${CMAKE_BINARY_DIR}
    COMMENT "Mozc の辞書データから toraime.dic を作ります"
    VERBATIM)
endif()
