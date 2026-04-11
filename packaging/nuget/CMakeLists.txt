monika_feature(Core)

function(monika_add_processed_xml target input output)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs FLAGS)

    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    cmake_path(GET input FILENAME INPUT_FILE_NAME)

    add_custom_command(
        OUTPUT "${output}"
        COMMAND ${CMAKE_C_COMPILER}
                    -E -P -traditional-cpp
                    -x c
                    ${ARG_FLAGS}
                    -DMONIKA_UNQUOTED_BUILD_PROPS
                    -include "${MONIKA_BUILD_PROPS_H}"
                    "${input}"
                    -o "${output}"
        COMMAND powershell -NoProfile -Command "
                    $raw = (Get-Content '${output}' -Raw).Trim();
                    $xml = [xml]$raw;
                    $settings = New-Object System.Xml.XmlWriterSettings;
                    $settings.Indent = $true;
                    $writer = [System.Xml.XmlWriter]::Create(
                        '${output}',
                        $settings
                    );
                    $xml.Save($writer);
                    $writer.Dispose()
                "
        DEPENDS
            UpdateBuildProps
            "${input}"
            ${MONIKA_BUILD_PROPS_H}
        COMMENT "Processing XML file ${INPUT_FILE_NAME}"
        VERBATIM
    )

    set_source_files_properties("${output}" PROPERTIES GENERATED TRUE)

    add_custom_target(
        ${target} ALL
        DEPENDS "${output}"
    )
endfunction()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(MONIKA_NUGET_PACKAGE_SUFFIX ".Debug")
else()
    set(MONIKA_NUGET_PACKAGE_SUFFIX "")
endif()

monika_add_processed_xml(
    MonikaNuGetXml
    "${CMAKE_CURRENT_SOURCE_DIR}/monika.nuspec"
    "${CMAKE_CURRENT_BINARY_DIR}/monika.nuspec"
    FLAGS
        "-DMONIKA_NUGET_PACKAGE_ID=monika${MONIKA_NUGET_PACKAGE_SUFFIX}"
)

set(MONIKA_NUGET_TARGETS_PATH "build/native/monika.SDK${MONIKA_NUGET_PACKAGE_SUFFIX}.targets")

monika_add_processed_xml(
    MonikaNuGetSdkXml
    "${CMAKE_CURRENT_SOURCE_DIR}/monika.SDK.nuspec"
    "${CMAKE_CURRENT_BINARY_DIR}/monika.SDK.nuspec"
    FLAGS
        "-DMONIKA_NUGET_PACKAGE_ID=monika.SDK${MONIKA_NUGET_PACKAGE_SUFFIX}"
        "-DMONIKA_NUGET_TARGETS_PATH=\"${MONIKA_NUGET_TARGETS_PATH}\""
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/monika.cmd.in"
    "${CMAKE_CURRENT_BINARY_DIR}/monika.cmd"
)

add_custom_target(MonikaNuGetStub ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/monika.cmd")

monika_should_install_target(MonikaNuGetXml MONIKA_NUGET_SHOULD_INSTALL)
if(MONIKA_NUGET_SHOULD_INSTALL)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/monika.nuspec"
        DESTINATION "${CMAKE_INSTALL_PREFIX}"
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/monika.SDK.nuspec"
        DESTINATION "${CMAKE_INSTALL_PREFIX}"
    )
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/DotnetToolSettings.xml"
        DESTINATION "tools/any/win-${MONIKA_NUGET_ARCH}"
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/monika.cmd"
        DESTINATION "tools/any/win-${MONIKA_NUGET_ARCH}"
    )
endif()
