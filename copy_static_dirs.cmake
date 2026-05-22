# Функция для копирования staticDirs
function(copy_static_dirs_from_config CONFIG_FILE TARGET_NAME)
    if(NOT EXISTS ${CONFIG_FILE})
        message(WARNING "Config file ${CONFIG_FILE} not found")
        return()
    endif()
    
    # Ищем Python
    find_package(Python3 QUIET)
    if(NOT Python3_Interpreter_FOUND)
        message(WARNING "Python3 not found, cannot copy static dirs")
        return()
    endif()
    
    # Создаём Python скрипт
    set(SCRIPT_FILE "${CMAKE_CURRENT_BINARY_DIR}/extract_static_dirs.py")
    file(WRITE ${SCRIPT_FILE} "
import json
import sys

try:
    with open('${CONFIG_FILE}', 'r') as f:
        config = json.load(f)
    
    # Берем значения из staticDirs (реальные имена папок)
    static_dirs = config.get('staticDirs', {})
    folders = list(static_dirs.values())
    
    # Выводим папки через null byte для безопасного парсинга
    for folder in folders:
        print(folder)
        
except Exception as e:
    print(f'Error: {e}', file=sys.stderr)
    sys.exit(1)
")
    
    # Запускаем скрипт и получаем список папок
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${SCRIPT_FILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE FOLDERS_RAW
        ERROR_VARIABLE PARSE_ERROR
        RESULT_VARIABLE PARSE_RESULT
    )
    
    if(NOT PARSE_RESULT EQUAL 0)
        message(WARNING "Failed to parse config: ${PARSE_ERROR}")
        return()
    endif()
    
    # Разбиваем на список
    string(STRIP "${FOLDERS_RAW}" FOLDERS_RAW)
    string(REPLACE "\n" ";" FOLDER_LIST "${FOLDERS_RAW}")
    
    # Копируем каждую папку
    foreach(FOLDER ${FOLDER_LIST})
        string(STRIP "${FOLDER}" FOLDER)
        if(FOLDER)
            set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${FOLDER}")
            if(EXISTS ${SOURCE_DIR})
                message(STATUS "Copying static directory: ${FOLDER}")
                add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                        ${SOURCE_DIR}
                        $<TARGET_FILE_DIR:${TARGET_NAME}>/${FOLDER}
                    COMMENT "Copying ${FOLDER} to ${TARGET_NAME} directory"
                )
            else()
                message(WARNING "Static directory '${FOLDER}' not found at ${SOURCE_DIR}")
            endif()
        endif()
    endforeach()
endfunction()