find_package(Corrade REQUIRED Main)
find_package(Magnum REQUIRED GL Shaders Sdl2Application)

file (GLOB CPP_FILES 
        *.cpp)

file (GLOB H_FILES *.h)


set (SOURCE_FILES ${CPP_FILES} ${H_FILES})

add_executable(osp-magnum ${CPP_FILES})

# Put executable in the bin folder
set_target_properties(osp-magnum PROPERTIES RUNTIME_OUTPUT_DIRECTORY ../bin)

target_link_libraries(osp-magnum PRIVATE
    Corrade::Main
    Magnum::Application
    Magnum::GL
    Magnum::Magnum
    Magnum::Shaders)


