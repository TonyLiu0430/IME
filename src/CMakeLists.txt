set(SOURCES
    DllMain.cpp
    Register.cpp
    ClassFactory.cpp
    TextService.cpp
    DebugSink.cpp
    ime.def
)

add_library(IME SHARED ${SOURCES})

target_compile_definitions(IME PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    WINVER=0x0601        # Windows 7+
    _WIN32_WINNT=0x0601
)

target_compile_options(IME PRIVATE
    /W4         # 高警告等級
    /WX         # 警告視為錯誤
    /utf-8      # 原始碼 UTF-8 編碼
)

target_include_directories(IME PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
)

find_package(wil CONFIG REQUIRED)

target_link_libraries(IME PRIVATE
    WIL::WIL
    advapi32    # Registry API
    ole32       # COM
    oleaut32    # COM Automation
    ws2_32      # Winsock2 (DebugSink)
)

set_target_properties(IME PROPERTIES
    OUTPUT_NAME "MyIME"
    PREFIX      ""
)
