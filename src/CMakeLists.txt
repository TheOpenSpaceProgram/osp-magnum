find_package(Corrade REQUIRED Main)
find_package(Magnum REQUIRED
    GL
    Shaders
    MeshTools
    Primitives
    SceneGraph
    Trade
    Sdl2Application
    )

find_package(MagnumPlugins REQUIRED
    TinyGltfImporter)

find_package(Threads)

file (GLOB_RECURSE CPP_FILES
        *.cpp)

file (GLOB_RECURSE H_FILES *.h)


set (SOURCE_FILES ${CPP_FILES} ${H_FILES})

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
    Magnum::SceneGraph
    Magnum::Shaders
    Magnum::Trade
    MagnumPlugins::TinyGltfImporter
    )
