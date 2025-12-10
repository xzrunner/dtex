cmake_minimum_required(VERSION 3.16)

project(dtex)

add_definitions(
    -DNO_BOOST
)

################################################################################
# Source groups
################################################################################
set(pixel
    "include/dtex/PixBufPage.h"
    "include/dtex/PixelBuffer.h"
    "source/PixBufPage.cpp"
    "source/PixelBuffer.cpp"
)
source_group("pixel" FILES ${pixel})

set(texture
    "include/dtex/TexBufBlock.h"
    "include/dtex/TexBufDrawTask.h"
    "include/dtex/TexBufNode.h"
    "include/dtex/TexBufPreNode.h"
    "include/dtex/TextureBuffer.h"
    "source/TexBufBlock.cpp"
    "source/TexBufDrawTask.cpp"
    "source/TexBufNode.cpp"
    "source/TexBufPreNode.cpp"
    "source/TextureBuffer.cpp"
)
source_group("texture" FILES ${texture})

set(utility
    "include/dtex/TexPacker.h"
    "include/dtex/TexRenderer.h"
    "include/dtex/Utility.h"
    "source/TexPacker.cpp"
    "source/TexRenderer.cpp"
)
source_group("utility" FILES ${utility})

set(ALL_FILES
    ${pixel}
    ${texture}
    ${utility}
)

################################################################################
# Target
################################################################################

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/external/unirender/include)
include_directories(${CMAKE_SOURCE_DIR}/external/texpack)
include_directories(${CMAKE_SOURCE_DIR}/external/shadertrans/include)

add_library(${PROJECT_NAME} STATIC ${ALL_FILES})
