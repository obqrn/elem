cmake_minimum_required(VERSION 3.16.0)
project(ElementsSampler LANGUAGES C CXX)

if (NOT ELEMENTS_ROOT)
   message(FATAL_ERROR "ELEMENTS_ROOT is not set")
endif()

# Make sure ELEMENTS_ROOT is an absolute path to add to the CMake module path
get_filename_component(ELEMENTS_ROOT "${ELEMENTS_ROOT}" ABSOLUTE)
set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${ELEMENTS_ROOT}/cmake")

# If we are building outside the project, you need to set ELEMENTS_ROOT:
if (NOT ELEMENTS_BUILD_EXAMPLES)
   include(ElementsConfigCommon)
   set(ELEMENTS_BUILD_EXAMPLES OFF)
   add_subdirectory(${ELEMENTS_ROOT} elements)
endif()

set(ELEMENTS_APP_PROJECT "ElementsSampler")
set(ELEMENTS_APP_TITLE "Elements Sampler")
set(ELEMENTS_APP_COPYRIGHT "Copyright (c) 2016-2024 Joel de Guzman")
set(ELEMENTS_APP_ID "com.cycfi.elements-sampler")
set(ELEMENTS_APP_VERSION "1.0")

set(ELEMENTS_APP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

# All resources (images/sprites) used by the aggregated pages, from the
# individual example folders. File names are unique so the flat copy into
# the resources directory works.
set(ELEMENTS_APP_RESOURCES
   ${CMAKE_CURRENT_SOURCE_DIR}/../buttons/resources/power_180x632.png
   ${CMAKE_CURRENT_SOURCE_DIR}/../buttons/resources/phase_180x790.png
   ${CMAKE_CURRENT_SOURCE_DIR}/../buttons/resources/mail_180x790.png
   ${CMAKE_CURRENT_SOURCE_DIR}/../buttons/resources/transpo_180x632.png
   ${CMAKE_CURRENT_SOURCE_DIR}/../scale/resources/phase_180x632.png
   ${CMAKE_CURRENT_SOURCE_DIR}/../scale/resources/mail_180x632.png
   ${CMAKE_CURRENT_SOURCE_DIR}/../sprite_sliders_and_knobs/resources/slider-white.png
   ${CMAKE_CURRENT_SOURCE_DIR}/../sprite_sliders_and_knobs/resources/knob_sprites_white_128x128.png
   ${CMAKE_CURRENT_SOURCE_DIR}/../custom_control/resources/knob_sprites_150x150_darker.png
   ${CMAKE_CURRENT_SOURCE_DIR}/../custom_control/resources/wall.jpg
   ${CMAKE_CURRENT_SOURCE_DIR}/../menus/resources/deep_space.jpg
   ${CMAKE_CURRENT_SOURCE_DIR}/../drop_file/resources/space.jpg
   ${CMAKE_CURRENT_SOURCE_DIR}/../simple_animation/resources/moving.png
   ${CMAKE_CURRENT_SOURCE_DIR}/../text_edit/resources/WenQuanYiMicroHei.ttf
)

# For your custom application icon on macOS or Windows see cmake/AppIcon.cmake module
include(AppIcon)
include(ElementsConfigApp)
