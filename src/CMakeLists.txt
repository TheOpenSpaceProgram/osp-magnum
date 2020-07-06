find_package(Corrade REQUIRED Main)
find_package(Magnum REQUIRED
    GL
    Shaders
    MeshTools
    Primitives
    #SceneGraph
    Trade
    Sdl2Application
    AnyImageImporter
    )

find_package(MagnumPlugins REQUIRED
    TinyGltfImporter)

find_package(Threads)

# Gather paths to OSP headers and sources
file (GLOB_RECURSE CPP_FILES *.cpp)
file (GLOB_RECURSE H_FILES *.h)
set (SOURCE_FILES ${CPP_FILES} ${H_FILES})

include_directories(.)

# Include Newton Dynamics to the project

# TODO: find a better way to do this.
#       Somehow Newton needs some defines for headers to work properly.
#       This section was based on Newton Dynamic's CMakeLists.txt
if(UNIX)
    if (BUILD_64)
        add_definitions(-D_POSIX_VER_64)
    else (BUILD_64)
        add_definitions(-D_POSIX_VER)
    endif (BUILD_64)

elseif(MSVC)
    if(WIN32)
        if(CMAKE_CL_64)
            add_definitions(-D_WIN_64_VER)
        else()
            add_definitions(-D_WIN_32_VER)
        endif()
    else()
        add_definitions(-D_ARM_VER)
    endif()

elseif(MINGW)
    if(CMAKE_CL_64)
        add_definitions(-D_MINGW_64_VER)
        add_definitions(-D_WIN_64_VER)
    else()
        add_definitions(-D_MINGW_32_VER)
        add_definitions(-D_WIN_32_VER)
    endif()

endif()

# this too
include_directories(../3rdparty/newton-dynamics/sdk/dMath/)
include_directories(../3rdparty/newton-dynamics/sdk/dgCore/)
include_directories(../3rdparty/newton-dynamics/sdk/dScene/)
include_directories(../3rdparty/newton-dynamics/sdk/dModel/)
include_directories(../3rdparty/newton-dynamics/sdk/dNewton/)
include_directories(../3rdparty/newton-dynamics/sdk/dgNewton/)
include_directories(../3rdparty/newton-dynamics/sdk/dVehicle/)
include_directories(../3rdparty/newton-dynamics/sdk/dgPhysics/)
include_directories(../3rdparty/newton-dynamics/sdk/dAnimation/)
include_directories(../3rdparty/newton-dynamics/sdk/dgMeshUtil/)
include_directories(../3rdparty/newton-dynamics/sdk/dContainers/)
include_directories(../3rdparty/newton-dynamics/sdk/dCustomJoints/)
include_directories(../3rdparty/newton-dynamics/sdk/thirdParty/tinyxml/)
include_directories(../3rdparty/newton-dynamics/sdk/dVehicle/dPlayerController)
include_directories(../3rdparty/newton-dynamics/sdk/dVehicle/dMultiBodyVehicle)


# Include ENTT (header only lib)
include_directories(../3rdparty/entt/src)


add_executable(osp-magnum ${CPP_FILES})

# Put executable in the bin folder
set_target_properties(osp-magnum PROPERTIES RUNTIME_OUTPUT_DIRECTORY ../bin)

target_link_libraries(osp-magnum PRIVATE
    ${CMAKE_THREAD_LIBS_INIT}
    Corrade::Main
    Magnum::Application
    Magnum::GL
    Magnum::Magnum
    Magnum::MeshTools
    Magnum::Primitives
    #Magnum::SceneGraph
    Magnum::Shaders
    Magnum::Trade
    Magnum::AnyImageImporter
    MagnumPlugins::TinyGltfImporter
    dNewton dScene dModel dVehicle
    )
