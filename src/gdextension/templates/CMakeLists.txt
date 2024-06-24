# SPDX-License-Identifier: Unlicense

add_custom_target( templates
    SOURCES
        template.debug.gdextension.in
        template.release.gdextension.in
)

add_dependencies( osp-gdextension templates )

# TODO make this a bit generic with some kind of template. Currently hardcoded.

set( GD_EXTENSION_FILE_INPUT osp.gdextension )

# Install the gdextension file
install(
    FILES ${GD_EXTENSION_FILE_INPUT}
    DESTINATION ${INSTALL_DIR}
)

unset( ALLOWED_BUILDS )
unset( BUILD_TYPE )
unset( GD_EXTENSION_FILE )
unset( GD_EXTENSION_FILE_INPUT )
unset( LIB_PREFIX )
