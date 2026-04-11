set(MONIKA_FEATURES "")

function(monika_add_feature FEATURE_NAME DEFAULT_INSTALL)
    string(TOUPPER ${FEATURE_NAME} FEATURE_NAME_UPPER)
    option(MONIKA_INSTALL_${FEATURE_NAME_UPPER} "Install ${FEATURE_NAME}" ${DEFAULT_INSTALL})

    list(APPEND MONIKA_FEATURES ${FEATURE_NAME})
    set(MONIKA_FEATURES "${MONIKA_FEATURES}" PARENT_SCOPE)
endfunction()

monika_add_feature(Core ON)
monika_add_feature(MXSS OFF)

define_property(DIRECTORY PROPERTY MONIKA_FEATURE
    INHERITED
    BRIEF_DOCS "The feature this directory belongs to."
)

macro(monika_feature FEATURE_NAME)
    if(NOT ${FEATURE_NAME} IN_LIST MONIKA_FEATURES)
        message(FATAL_ERROR "Unknown feature: ${FEATURE_NAME}")
    endif()
    set_directory_properties(PROPERTIES MONIKA_FEATURE ${FEATURE_NAME})
endmacro()

function(_monika_get_target_feature name RESULT_VAR)
    get_target_property(FEATURE_DIRECTORY ${name} SOURCE_DIR)
    get_directory_property(FEATURE_NAME DIRECTORY ${FEATURE_DIRECTORY} MONIKA_FEATURE)

    set(${RESULT_VAR} "${FEATURE_NAME}" PARENT_SCOPE)
endfunction()

function(monika_should_install_target name RESULT_VAR)
    _monika_get_target_feature(${name} FEATURE_NAME)

    string(TOUPPER "${FEATURE_NAME}" FEATURE_NAME_UPPER)

    set(${RESULT_VAR} ${MONIKA_INSTALL_${FEATURE_NAME_UPPER}} PARENT_SCOPE)
endfunction()

function(_monika_get_all_targets RESULT_VAR)
    macro(_monika_get_all_targets_helper TARGET_DIRECTORY RESULT_VAR)
        get_property(current_targets DIRECTORY "${TARGET_DIRECTORY}" PROPERTY BUILDSYSTEM_TARGETS)
        list(APPEND ${RESULT_VAR} ${current_targets})

        get_property(subdirs DIRECTORY "${TARGET_DIRECTORY}" PROPERTY SUBDIRECTORIES)
        foreach(dir IN LISTS subdirs)
            _monika_get_all_targets_helper("${dir}" ${RESULT_VAR})
        endforeach()
    endmacro()

    set(targets)
    _monika_get_all_targets_helper("${CMAKE_CURRENT_SOURCE_DIR}" targets)
    set(${RESULT_VAR} ${targets} PARENT_SCOPE)
endfunction()

function(_monika_add_feature_target name)
    set(FEATURE_TARGET_NAME "Feature${name}")
    add_custom_target(${FEATURE_TARGET_NAME})

    _monika_get_all_targets(targets)

    foreach(target IN LISTS targets)
        _monika_get_target_feature(${target} feature)

        if(feature STREQUAL name)
            add_dependencies(${FEATURE_TARGET_NAME} ${target})
        endif()
    endforeach()
endfunction()

function(monika_add_feature_targets)
    foreach(feature IN LISTS MONIKA_FEATURES)
        _monika_add_feature_target(${feature})
    endforeach()
endfunction()
