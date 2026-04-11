# CMake does not recognize PDB generator properties for clang in MinGW mode.
function(_monika_get_target_pdb_name name pdb_name)
    set(${pdb_name} "$<TARGET_FILE_DIR:${name}>/$<TARGET_FILE_BASE_NAME:${name}>.pdb" PARENT_SCOPE)
endfunction()

function(monika_target_pdb name)
    _monika_get_target_pdb_name(${name} PDB_NAME)

    # Enable PDBs for use with VS Code Debugger.
    target_compile_options(${name} PRIVATE -gcodeview)

    set_property(TARGET ${name} APPEND PROPERTY
        ADDITIONAL_CLEAN_FILES ${PDB_NAME}
    )
endfunction()

function(monika_target_mingw_pdb name)
    _monika_get_target_pdb_name(${name} PDB_NAME)

    target_link_options(${name} PRIVATE -Wl,--pdb=${PDB_NAME})

    monika_target_pdb(${name})
endfunction()

function(monika_install_pdb name)
    _monika_get_target_pdb_name(${name} PDB_NAME)

    install(FILES ${PDB_NAME} DESTINATION Debug/${MONIKA_INSTALL_ARCH})
endfunction()
