cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_p4_dashboard)

set(DASHBOARD_COMPONENT_PATCH_SCRIPT "${CMAKE_SOURCE_DIR}/tools/apply_component_patches.py")
if(EXISTS "${DASHBOARD_COMPONENT_PATCH_SCRIPT}")
    if(NOT DEFINED PYTHON)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        set(PYTHON "${Python3_EXECUTABLE}")
    endif()
    execute_process(
        COMMAND "${PYTHON}" "${DASHBOARD_COMPONENT_PATCH_SCRIPT}" --quiet
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()
