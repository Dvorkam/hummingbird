cmake_minimum_required(VERSION 3.20)
project(Hummingbird)

# Set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(HB_LOG_LEVEL "ERROR" CACHE STRING "Log level: OFF, ERROR, WARN, INFO, DEBUG")
set_property(CACHE HB_LOG_LEVEL PROPERTY STRINGS OFF ERROR WARN INFO DEBUG)

string(TOUPPER "${HB_LOG_LEVEL}" HB_LOG_LEVEL_UPPER)
set(HB_LOG_LEVEL_NUM 1)
if(HB_LOG_LEVEL_UPPER STREQUAL "OFF")
    set(HB_LOG_LEVEL_NUM 0)
elseif(HB_LOG_LEVEL_UPPER STREQUAL "ERROR")
    set(HB_LOG_LEVEL_NUM 1)
elseif(HB_LOG_LEVEL_UPPER STREQUAL "WARN")
    set(HB_LOG_LEVEL_NUM 2)
elseif(HB_LOG_LEVEL_UPPER STREQUAL "INFO")
    set(HB_LOG_LEVEL_NUM 3)
elseif(HB_LOG_LEVEL_UPPER STREQUAL "DEBUG")
    set(HB_LOG_LEVEL_NUM 4)
else()
    message(FATAL_ERROR "Unknown HB_LOG_LEVEL: ${HB_LOG_LEVEL}")
endif()
add_compile_definitions(HB_LOG_LEVEL=${HB_LOG_LEVEL_NUM})

find_package(SDL2 REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(blend2d REQUIRED)
find_package(CURL REQUIRED)
find_package(qjs CONFIG REQUIRED)
find_package(lunasvg CONFIG REQUIRED)

# --- Style Library ---
add_library(Style STATIC
    src/style/parser/CssTokenizer.cpp
    src/style/parser/CssParser.cpp
    src/style/registry/CssPropertyRegistry.cpp
    src/style/selector/SelectorMatcher.cpp
    src/style/compute/StyleDefaults.cpp
    src/style/compute/StyleEngine.cpp
    src/style/compute/StyleValueUtils.cpp
    src/style/compute/StylesheetSource.cpp
    src/style/compute/apply/ApplyBackground.cpp
    src/style/compute/apply/ApplyColorUtils.cpp
    src/style/compute/apply/ApplyLayout.cpp
    src/style/compute/apply/ApplyText.cpp
    src/style/compute/apply/PropertyApplier.cpp
)
target_include_directories(Style
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# --- Core Library ---
# This library will contain the abstract interfaces and platform-independent logic.
add_library(Core STATIC
    src/core/ArenaAllocator.cpp
    src/core/dom/DomFactory.cpp
    src/core/utils/AssetLoader.cpp
    src/core/utils/AssetPath.cpp
    src/core/utils/TextEditBuffer.cpp
    src/core/utils/Url.cpp
)
target_include_directories(Core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# --- Platform Library ---
# This library will contain the platform-specific implementations (e.g., SDL2, Blend2D).
add_library(Platform STATIC
    src/platform/graphics/SDLWindow.cpp
    src/platform/graphics/SDLGraphicsContext.cpp
    src/platform/graphics/Blend2DFontCache.cpp
    src/platform/graphics/SDLInputTranslation.cpp
    src/platform/net/CurlNetwork.cpp
    src/platform/net/StubNetwork.cpp
    src/platform/net/NetworkFactory.cpp
    src/platform/decoders/SDLImageDecoder.cpp
    src/platform/decoders/SvgImageDecoder.cpp
    src/platform/decoders/ImageDecodeUtils.cpp
    src/platform/decoders/CompositeImageDecoder.cpp
    src/platform/decoders/ImageDecoderFactory.cpp
    src/platform/resources/FileResourceProvider.cpp
    src/platform/resources/ResourceProviderFactory.cpp
    src/platform/graphics/SDLWindowFactory.cpp
    src/platform/net/NetworkThreadPool.cpp
    src/platform/script/NullScriptEngine.cpp
    src/platform/script/QuickJSScriptEngine.cpp
    src/platform/script/ScriptEngineFactory.cpp
)
target_include_directories(Platform
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Platform PUBLIC Core SDL2::SDL2
    $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
    blend2d::blend2d CURL::libcurl qjs lunasvg::lunasvg)

# --- HTML Parser Library ---
add_library(Html STATIC
    src/html/HtmlTokenizer.cpp
    src/html/HtmlParser.cpp
    src/html/HtmlEntityDecoder.cpp
)
target_include_directories(Html
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Html PUBLIC Core)

# --- Layout Library ---
add_library(Layout STATIC
    src/layout/RenderObject.cpp
    src/layout/geometry/PositioningUtils.cpp
    src/layout/RenderFactory.cpp
    src/layout/TreeBuilder.cpp
    src/layout/block/BlockBox.cpp
    src/layout/block/FloatLayoutUtils.cpp
    src/layout/flow/InlineBox.cpp
    src/layout/flow/TextBox.cpp
    src/layout/flow/InlineLineBuilder.cpp
    src/layout/flow/FlowLayoutUtils.cpp
    src/layout/flow/inline/InlineLayoutUtils.cpp
    src/layout/formatting/RenderListItem.cpp
    src/layout/controls/RenderBreak.cpp
    src/layout/controls/RenderRule.cpp
    src/layout/replaced/RenderImage.cpp
    src/layout/replaced/RenderSvg.cpp
    src/layout/replaced/ReplacedSizingUtils.cpp
    src/layout/table/RenderTable.cpp
)
target_include_directories(Layout
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Layout PUBLIC Core Style)

# --- Renderer Library ---
add_library(Renderer STATIC
    src/renderer/DisplayList.cpp
    src/renderer/Painter.cpp
    src/renderer/RenderCommandUtils.cpp
)
target_include_directories(Renderer
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# --- Engine Library ---
# This library owns document lifecycle and orchestration (parsing/style/layout/paint) without platform headers.
add_library(Engine STATIC
    src/engine/document/DocumentPipeline.cpp
    src/engine/document/DocumentInputController.cpp
    src/engine/document/DocumentModel.cpp
    src/engine/document/DocumentResources.cpp
    src/engine/document/DocumentPainter.cpp
    src/engine/extensions/ExtensionManifest.cpp
    src/engine/script/DocumentScriptController.cpp
    src/engine/script/DocumentScriptHost.cpp
    src/engine/resources/ResourceLoader.cpp
    src/engine/tab/Tab.cpp
    src/engine/tab/TabManager.cpp
    src/engine/resources/ResourceStore.cpp
)
target_include_directories(Engine
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Engine PUBLIC Core Html Layout Renderer Style)

# --- App Library ---
add_library(App
    src/app/BrowserApp.cpp
    src/app/BrowserApp.h
    src/app/TabStrip.cpp
    src/app/TabStrip.h
    src/app/UrlBar.cpp
    src/app/UrlBar.h
)
target_include_directories(App PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(App PUBLIC Platform Engine)

# --- App Executable ---
# This is the main application that brings everything together.
add_executable(Hummingbird
    src/app/main.cpp
)
if(WIN32)
    target_sources(Hummingbird PRIVATE src/app/hummingbird.rc)
endif()

target_link_libraries(Hummingbird PRIVATE App SDL2::SDL2main)

# --- Unit Testing ---
enable_testing()
add_subdirectory(tests)
