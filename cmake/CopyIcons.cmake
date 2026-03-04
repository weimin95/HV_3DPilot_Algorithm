if(NOT DEFINED ICON_SRC_ROOT OR NOT DEFINED ICON_DST_ROOT)
    message(FATAL_ERROR "ICON_SRC_ROOT and ICON_DST_ROOT must be provided")
endif()

if(NOT EXISTS "${ICON_SRC_ROOT}")
    message(FATAL_ERROR "Icon source root does not exist: ${ICON_SRC_ROOT}")
endif()

file(GLOB_RECURSE ICON_FILES "${ICON_SRC_ROOT}/*.png")

foreach(ICON_FILE IN LISTS ICON_FILES)
    file(RELATIVE_PATH ICON_REL_PATH "${ICON_SRC_ROOT}" "${ICON_FILE}")
    get_filename_component(ICON_REL_DIR "${ICON_REL_PATH}" DIRECTORY)
    if(ICON_REL_DIR)
        file(MAKE_DIRECTORY "${ICON_DST_ROOT}/${ICON_REL_DIR}")
    else()
        file(MAKE_DIRECTORY "${ICON_DST_ROOT}")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${ICON_FILE}" "${ICON_DST_ROOT}/${ICON_REL_PATH}"
        RESULT_VARIABLE COPY_RET
    )
    if(NOT COPY_RET EQUAL 0)
        message(FATAL_ERROR "Failed to copy icon: ${ICON_FILE}")
    endif()
endforeach()
