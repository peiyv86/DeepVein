cmake_minimum_required(VERSION 3.16)
project(DeepVein LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql Concurrent Pdf PdfWidgets LinguistTools)
set(TS_FILES
    "i18n/lang_zh_CN.ts"
    "i18n/lang_en_US.ts"
)
qt_add_translations(DeepVein TS_FILES ${TS_FILES})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/include)

# Pandoc 执行路径
add_definitions(-DPANDOC_PATH="${CMAKE_CURRENT_SOURCE_DIR}/third_party/bin/pandoc.exe")
add_definitions(-DNOMINMAX)

file(GLOB_RECURSE PROJECT_SOURCES "src/*.cpp" "src/*.h" "src/*.ui")
file(GLOB_RECURSE RAPIDOCR_SOURCES "third_party/src/rapidocr_cpp/src/*.cpp")
file(GLOB PUGIXML_SOURCES "third_party/src/pugixml/*.cpp")


set(PROJECT_RESOURCES "resources/app.qrc")
set(WIN_RESOURCES "resources/img/logo.rc")

add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCES}
    ${RAPIDOCR_SOURCES}
    ${PUGIXML_SOURCES}
    ${PROJECT_RESOURCES}
    ${WIN_RESOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Sql Qt6::Concurrent Qt6::Pdf Qt6::PdfWidgets
)

if(MSVC)
    file(GLOB THIRD_PARTY_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib/*.lib")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${THIRD_PARTY_LIBS}
        ntdll bcrypt ws2_32 userenv advapi32 # Windows 底层依赖
    )
    file(GLOB RUNTIME_DLLS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bin/*.dll")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${RUNTIME_DLLS}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/models"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/models"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/models"
    )
endif()
