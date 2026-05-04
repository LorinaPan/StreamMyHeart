set(PLUGIN_AUTHOR "StreamMyHeart")
set(PLUGIN_URL "https://github.com/Maciek03102003/StreamMyHeart")

cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" OFF)
option(ENABLE_DEBUG_FEATURES "Enable debug instrumentation and experimental toggles" OFF)

option(BUILD_OBS_PLUGIN "Build as OBS plugin" ON)
option(BUILD_EVALUATION "Build the offline evaluation tool" OFF)

if(APPLE AND CMAKE_GENERATOR STREQUAL "Xcode")
  if(EXISTS "/usr/local/lib/libopencv_core.4.11.0.dylib" AND NOT EXISTS "/opt/homebrew/lib/libopencv_core.4.11.0.dylib")
    message(STATUS "Only x86_64 Homebrew OpenCV detected; forcing macOS build to x86_64.")
    set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "macOS target architectures" FORCE)
  endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

if(UNIX)
  set(CMAKE_C_FLAGS "-Wall -std=c99 -fPIC -Wno-unused-parameter -Wno-unused-private-field")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections")
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -g")
  set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS} -Os -DNDEBUG -fvisibility=hidden")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O3 -DNDEBUG -fvisibility=hidden")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS} -O2 -g -fvisibility=hidden")

  set(CMAKE_CXX_FLAGS "-Wall -ffunction-sections -fdata-sections -fPIC -Wno-unused-parameter -Wno-unused-private-field")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g")
  set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS} -Os -DNDEBUG -fvisibility=hidden")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG -fvisibility=hidden")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} -O2 -g -fvisibility=hidden")
endif()

message(
  "=======CMAKE cur_ver: ${CMAKE_VERSION}, cur_build_type:${CMAKE_BUILD_TYPE}, CMAKE_SYSTEM_NAME:${CMAKE_SYSTEM_NAME}"
)

include(compilerconfig)
include(defaults)
include(helpers)

if(BUILD_OBS_PLUGIN)
  add_library(${CMAKE_PROJECT_NAME} MODULE)
else()
  add_executable(${CMAKE_PROJECT_NAME})
endif()

add_library(streammyheart_core STATIC)

find_package(libobs REQUIRED)
target_include_directories(
  streammyheart_core
  PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/obs
    ${CMAKE_SOURCE_DIR}/src/obs/plugin
    ${CMAKE_SOURCE_DIR}/src/obs/monitor
    ${CMAKE_SOURCE_DIR}/src/obs/display
    ${CMAKE_SOURCE_DIR}/src/obs/runtime
)
target_link_libraries(streammyheart_core PUBLIC OBS::libobs)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs streammyheart_core)
target_include_directories(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/obs
    ${CMAKE_SOURCE_DIR}/src/obs/plugin
    ${CMAKE_SOURCE_DIR}/src/obs/monitor
    ${CMAKE_SOURCE_DIR}/src/obs/display
    ${CMAKE_SOURCE_DIR}/src/obs/runtime
)

find_package(Eigen3 REQUIRED)
include_directories(${EIGEN3_INCLUDE_DIR})
target_link_libraries(streammyheart_core PUBLIC Eigen3::Eigen)

if(UNIX)
  message("Configuring OpenCV for MacOS and Linux")
  set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/binary/opencv-4.11.0-macos/lib/cmake/opencv4)
elseif(WIN32)
  set(OpenCV_STATIC ON)
  set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/binary/opencv-4.11.0-windows)
endif()

find_package(OpenCV CONFIG REQUIRED)
if(OpenCV_FOUND)
  message("OpenCV_FOUND:${OpenCV_FOUND}")
  message("OpenCV_VERSION:${OpenCV_VERSION}")
  message("OpenCV_INCLUDE_DIRS:${OpenCV_INCLUDE_DIRS}")
  message("OpenCV_LIBRARIES:${OpenCV_LIBRARIES}")
endif()

include_directories(${OpenCV_INCLUDE_DIR})
link_directories(${OpenCV_LIBRARIES})
target_link_libraries(streammyheart_core PUBLIC ${OpenCV_LIBRARIES})

# Apply compiler options to ignore Dlib-specific warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
  target_compile_options(
    streammyheart_core
    PRIVATE
      -Wno-newline-eof # Ignore missing newline at end of file
      -Wno-unused-but-set-variable # Ignore unused variables
      -Wno-pessimizing-move # Ignore pessimizing move warnings
      -Wno-comma # Ignore comma operator warnings
      -Wno-error # Do not treat warnings as errors
  )
endif()

if(UNIX)
  # Set paths for Dlib
  set(DLIB_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/dlib-19.24.6)
  set(DLIB_ALL_SOURCE ${DLIB_INCLUDE_DIR}/dlib/all/source.cpp)

  if(NOT EXISTS "${DLIB_INCLUDE_DIR}/dlib/image_processing.h")
    message(FATAL_ERROR "Missing Dlib headers in ${DLIB_INCLUDE_DIR}. Run: git submodule update --init --recursive")
  endif()

  if(NOT EXISTS "${DLIB_ALL_SOURCE}")
    message(FATAL_ERROR "Missing Dlib all/source.cpp in ${DLIB_ALL_SOURCE}. Check the Dlib submodule contents.")
  endif()

  target_include_directories(streammyheart_core SYSTEM PUBLIC ${DLIB_INCLUDE_DIR})
  target_compile_definitions(
    streammyheart_core
    PUBLIC
      DLIB_NO_GUI_SUPPORT
  )
elseif(WIN32)
  set(dlib_STATIC ON)
  set(dlib_DIR ${CMAKE_SOURCE_DIR}/3rdparty/dlib-19.24.6/install/lib/cmake/dlib)

  find_package(dlib CONFIG REQUIRED)
  if(dlib_FOUND)
    message("dlib_FOUND:${dlib_FOUND}")
    message("dlib_VERSION:${dlib_VERSION}")
    message("dlib_INCLUDE_DIRS:${dlib_INCLUDE_DIRS}")
    message("dlib_LIBRARIES:${dlib_LIBRARIES}")
  endif()

  target_link_libraries(streammyheart_core PUBLIC dlib::dlib)
endif()

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(
  streammyheart_core
  PRIVATE
    3rdparty/dlib-19.24.6/dlib/all/source.cpp
    3rdparty/dlib-19.24.6/dlib/fft/fft.cpp
    src/algorithm/heart_rate_algorithm.cpp
    src/algorithm/face_detection/face_detection.cpp
    src/algorithm/face_detection/opencv_haarcascade.cpp
    src/algorithm/face_detection/opencv_dlib_68_landmarks_face_tracker.cpp
    src/core/heart_rate_pipeline.cpp
    src/algorithm/filtering/pre_filters.cpp
    src/algorithm/filtering/post_filters.cpp
    src/algorithm/filtering/filter_util.cpp
)

set_source_files_properties(
  3rdparty/dlib-19.24.6/dlib/all/source.cpp
  PROPERTIES COMPILE_OPTIONS "-Wno-sign-compare"
)

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/plugin-support.c
    src/obs/display/graph_render_model.cpp
    src/obs/display/graph_source.cpp
    src/obs/display/graph_source_info.c
    src/obs/display/display_scene.cpp
    src/obs/runtime/frame_capture.cpp
    src/obs/plugin/plugin-main.cpp
    src/obs/runtime/plugin_config.cpp
    src/obs/monitor/heart_rate_source.cpp
    src/obs/monitor/heart_rate_source_info.c
    src/obs/runtime/obs_utils.cpp
)

if(ENABLE_DEBUG_FEATURES)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE STREAM_MY_HEART_ENABLE_DEBUG_FEATURES=1)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

if(BUILD_EVALUATION)
  add_executable(${CMAKE_PROJECT_NAME}_eval src/eval/run_evaluation.cpp)
  target_include_directories(
    ${CMAKE_PROJECT_NAME}_eval
    PRIVATE
      ${CMAKE_SOURCE_DIR}/src
      ${CMAKE_SOURCE_DIR}/src/obs
      ${CMAKE_SOURCE_DIR}/src/obs/plugin
      ${CMAKE_SOURCE_DIR}/src/obs/monitor
      ${CMAKE_SOURCE_DIR}/src/obs/display
      ${CMAKE_SOURCE_DIR}/src/obs/runtime
  )
  target_compile_definitions(${CMAKE_PROJECT_NAME}_eval PRIVATE STREAM_MY_HEART_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  target_link_libraries(${CMAKE_PROJECT_NAME}_eval PRIVATE streammyheart_core)
endif()
