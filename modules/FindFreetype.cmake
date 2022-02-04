
if(TARGET freetype)
    get_target_property(FREETYPE_SOURCE_DIR freetype SOURCE_DIR)
    get_target_property(FREETYPE_INCLUDE_DIRS freetype INCLUDE_DIRECTORIES)
    set(FREETYPE_LIBRARIES freetype)
else()
endif()
