# PostInstall.cmake
# This script is executed at install time (cmake --install ...).
# It persists LOX2_HOME and updates the user's PATH to include the
# target output directory on Windows, and performs user-level installation
# steps on Unix.

if(WIN32)
    message(STATUS "Windows install post-actions: persisting LOX2_HOME and updating user PATH if required")

    # Determine target architecture
    if(DEFINED CMAKE_VS_PLATFORM_NAME AND NOT CMAKE_VS_PLATFORM_NAME STREQUAL "")
        set(_target "${CMAKE_VS_PLATFORM_NAME}")
    else()
        # default to Release when config is not provided
        set(_target "x64")		
    endif()

    # Determine configuration name (available during install as CMAKE_INSTALL_CONFIG_NAME).
    if(DEFINED CMAKE_INSTALL_CONFIG_NAME)
        set(_config "${CMAKE_INSTALL_CONFIG_NAME}")
    else()
        # default to Release when config is not provided
        set(_config "Release")
    endif()

    # Set the target output directory where the built executable lives.
    file(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}/${_target}/${_config}" _install_bin)
    file(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}" _home_dir)

    # Persist LOX2_HOME to point to the project source directory (where
    # the root CMakeLists.txt lives). Use CMAKE_INSTALL_PREFIX which references
    # the top-level source directory rather than the script directory.
    execute_process(
        COMMAND setx LOX2_HOME "${_home_dir}"
        RESULT_VARIABLE _setx_res
        OUTPUT_VARIABLE _setx_out
        ERROR_VARIABLE _setx_err
    )
    if(NOT _setx_res EQUAL 0)
        message(WARNING "Failed to set LOX2_HOME via setx: ${_setx_err}")
    else()
        message(STATUS "LOX2_HOME environment variable set to ${_home_dir} for current user (new processes will see it).")
    endif()

    # Update the per-user PATH only (do not copy system PATH into user PATH).
    # Prefer PowerShell if available, otherwise fall back to reg.exe.
    execute_process(
        COMMAND powershell -NoProfile -Command
            "try { $u=[Environment]::GetEnvironmentVariable('PATH','User'); if (-not $u) { [Environment]::SetEnvironmentVariable('PATH','${_install_bin}','User'); exit 0 } ; if ($u -notlike '*${_install_bin}*') { [Environment]::SetEnvironmentVariable('PATH', $u + ';${_install_bin}','User') } ; exit 0 } catch { exit 1 }"
        RESULT_VARIABLE _ps_res
        OUTPUT_VARIABLE _ps_out
        ERROR_VARIABLE _ps_err
    )

    if(_ps_res EQUAL 0)
        message(STATUS "User PATH updated to include ${_install_bin} via PowerShell (if it was not already present).")
    else()
        message(STATUS "PowerShell not available or failed (${_ps_err}). Falling back to reg.exe to modify HKCU\\Environment\\PATH.")

        execute_process(
            COMMAND cmd /c "reg query \"HKCU\\Environment\" /v PATH"
            RESULT_VARIABLE _reg_query_res
            OUTPUT_VARIABLE _reg_query_out
            ERROR_QUIET
        )

        if(_reg_query_res EQUAL 0 AND NOT _reg_query_out STREQUAL "")
            string(REGEX MATCH "PATH\\s+REG_\\w+\\s+(.*)" _match_line "${_reg_query_out}")
            if(CMAKE_MATCH_COUNT GREATER 0)
                set(_user_path "${CMAKE_MATCH_1}")
            else()
                set(_user_path "")
            endif()
        else()
            set(_user_path "")
        endif()

        if(_user_path STREQUAL "")
            execute_process(
                COMMAND cmd /c "reg add \"HKCU\\Environment\" /v PATH /t REG_EXPAND_SZ /d \"${_install_bin}\" /f"
                RESULT_VARIABLE _reg_add_res
                OUTPUT_VARIABLE _reg_add_out
                ERROR_VARIABLE _reg_add_err
            )
        else()
            string(FIND "${_user_path}" "${_install_bin}" _found_in_user_path)
            if(_found_in_user_path EQUAL -1)
                set(_new_user_path "${_user_path};${_install_bin}")
                execute_process(
                    COMMAND cmd /c "reg add \"HKCU\\Environment\" /v PATH /t REG_EXPAND_SZ /d \"${_new_user_path}\" /f"
                    RESULT_VARIABLE _reg_add_res
                    OUTPUT_VARIABLE _reg_add_out
                    ERROR_VARIABLE _reg_add_err
                )
            else()
                message(STATUS "Install bin directory ${_install_bin} already present in user PATH; skipping update.")
                set(_reg_add_res 0)
            endif()
        endif()

        if(DEFINED _reg_add_res AND NOT _reg_add_res EQUAL 0)
            message(WARNING "Failed to update user PATH via reg.exe: ${_reg_add_err}")
        elseif(DEFINED _reg_add_res)
            message(STATUS "User PATH updated to include ${_install_bin} via registry (if it was not already present). New shells will see the change.")
        endif()

    endif()

elseif(UNIX)
    if(DEFINED ENV{HOME})
        set(_home_dir "$ENV{HOME}")
        set(_shell_path "$ENV{SHELL}")
        if(_shell_path)
            get_filename_component(_shell_name "${_shell_path}" NAME)
        else()
            set(_shell_name "")
        endif()
        if(_shell_name STREQUAL "bash")
            set(_profile_file "${_home_dir}/.bashrc")
        elseif(_shell_name STREQUAL "zsh")
            set(_profile_file "${_home_dir}/.zshrc")
        else()
            set(_profile_file "${_home_dir}/.profile")
        endif()

        if(EXISTS "${_profile_file}")
            file(READ "${_profile_file}" _profile_contents)
        else()
            # file may not exist; treat as empty
            set(_profile_contents "")
        endif()
        string(FIND "${_profile_contents}" "LOX2_HOME" _found)
        if(_found EQUAL -1)
            # Persist LOX2_HOME to the project source directory for consistency
            # with the value set in the top-level CMakeLists.txt
            file(APPEND "${_profile_file}" "\n# Set LOX2_HOME by CMake\nexport LOX2_HOME=\"${CMAKE_SOURCE_DIR}\"\n")
            message(STATUS "Appended LOX2_HOME export to ${_profile_file}. New shells will see the variable.")
        else()
            message(STATUS "LOX2_HOME already present in ${_profile_file}; skipping.")
        endif()
    else()
        message(WARNING "HOME not defined; cannot persist LOX2_HOME on Unix")
    endif()

    # Attempt to copy only the installed executable to /usr/local/bin so it can
    # be invoked system-wide as: Lox2 --args
    set(_src_exe "${CMAKE_INSTALL_PREFIX}/bin/${PROJECT_NAME}")
    set(_dst_exe "/usr/local/bin/${PROJECT_NAME}")
    if(EXISTS "${_src_exe}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E copy "${_src_exe}" "${_dst_exe}"
            RESULT_VARIABLE _copy_res
            OUTPUT_VARIABLE _copy_out
            ERROR_VARIABLE _copy_err
        )
        if(_copy_res EQUAL 0)
            execute_process(COMMAND ${CMAKE_COMMAND} -E chmod 0755 "${_dst_exe}")
            message(STATUS "Copied ${_src_exe} to ${_dst_exe} and set executable permissions.")
        else()
            message(WARNING "Failed to copy ${_src_exe} to ${_dst_exe}: ${_copy_err}. You may need to run: sudo cmake --install . --prefix /usr/local or manually copy the file with sudo.")
        endif()
    else()
        message(STATUS "Source executable ${_src_exe} not found; skipping copy to /usr/local/bin")
    endif()

else()
    message(WARNING "Unsupported platform for persisting LOX2_HOME")
endif()
