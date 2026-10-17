file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

add_library(engine STATIC
    ${ENGINE_SOURCES}
    ${ENGINE_HEADERS}
)

target_compile_features(engine PUBLIC
    c_std_17
    cxx_std_20
)

target_include_directories(engine
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# GLFW
set(PLATFORM_SUPPORTS_GLFW FALSE)

if(WIN32 OR APPLE OR (UNIX AND NOT APPLE))
    set(PLATFORM_SUPPORTS_GLFW TRUE)
endif()

if(PLATFORM_SUPPORTS_GLFW)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/glfw")

    target_link_libraries(engine PUBLIC glfw)

    target_compile_definitions(engine PUBLIC
        WINDOW_BACKEND_GLFW_AVAILABLE
    )
endif()

# DirectX 12
if(WIN32)
    target_link_libraries(engine PUBLIC
        d3d12
        dxgi
        dxguid
        d3dcompiler
    )

    target_compile_definitions(engine PUBLIC
        RHI_BACKEND_DX12_AVAILABLE
    )
endif()

# Vulkan
if(WIN32 OR UNIX)
    target_compile_definitions(engine PUBLIC
        RHI_BACKEND_VULKAN_AVAILABLE
    )
endif()