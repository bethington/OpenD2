file(GLOB_RECURSE D2CLIENT_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

source_group("UI/Menus" "${CMAKE_CURRENT_SOURCE_DIR}/UI/Menus")
source_group("UI/Panels" "${CMAKE_CURRENT_SOURCE_DIR}/UI/Panels")
source_group("UI/Widgets" "${CMAKE_CURRENT_SOURCE_DIR}/UI/Widgets")
source_group("UI" "${CMAKE_CURRENT_SOURCE_DIR}/UI")
source_group("Shared" "${SHARED_SRC}")

add_library(D2Client SHARED
    "${D2CLIENT_FILES}"
    "${SHARED_SRC}"
)
set_target_properties(D2Client 
    PROPERTIES 
    OUTPUT_NAME D2Client
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
)
set_target_properties(D2Client PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(D2Client ${STATIC_LIBRARIES} D2Common)
target_compile_definitions(D2Client PRIVATE D2CLIENT)

if(USE_ALLEGRO5)
    target_compile_definitions(D2Client PRIVATE USE_ALLEGRO5 ALLEGRO_NO_MAGIC_MAIN)
    if(WIN32)
        set(ALLEGRO5_LIB_DIR "${CMAKE_SOURCE_DIR}/Libraries/allegro5/lib/win32")
        target_include_directories(D2Client PRIVATE
            "${CMAKE_SOURCE_DIR}/Libraries/allegro5/include"
        )
        target_link_libraries(D2Client
            "${ALLEGRO5_LIB_DIR}/allegro.lib"
            "${ALLEGRO5_LIB_DIR}/allegro_font.lib"
            "${ALLEGRO5_LIB_DIR}/allegro_primitives.lib"
        )
    else()
        find_package(Allegro5 REQUIRED)
        target_include_directories(D2Client PRIVATE ${ALLEGRO5_INCLUDE_DIR})
        target_link_libraries(D2Client ${ALLEGRO5_LIBRARIES})
    endif()
endif()
if(ENABLE_EDITOR)
    target_compile_definitions(D2Client PRIVATE ENABLE_EDITOR)
endif()
