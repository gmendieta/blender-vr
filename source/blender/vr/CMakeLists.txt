# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Gorka Mendieta.
#
# ***** END GPL LICENSE BLOCK *****

# WITH_OPENGL limits the visibility of the opengl headers to just GPU and bg_gpu,
# to more easily highlight codepadths in other libraries that need to be refactored,
# bf_gpu is allowed to have opengl regardless of this option.

set(INC
    .
    ./intern
    
    ../blenkernel
    ../blenlib
    ../blentranslation
    ../depsgraph
    ../draw
    ../draw/intern
    ../editors/include
	../editors/gizmo_library
	../editors/gpencil
	../editors/transform
    ../gpu
    ../makesdna
	../makesrna
    ../render/extern/include
    ../windowmanager
    
    ../../../extern/glew/include/GL
    ../../../intern/glew-mx
    ../../../intern/guardedalloc
)
    
set(INC_SYS
    ${GLEW_INCLUDE_PATH}
    ${VR_INCLUDE_PATH}
)

set(SRC
    intern/vr_vr.cpp
    intern/vr_math.cpp
    intern/vr_oculus.cpp

    vr_build.h
    vr_api.h
    vr_vr.h
    
    intern/vr_math.h
    intern/vr_oculus.h
    intern/vr_bitmask.h
    intern/vr_utils.h
)

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_vr "${SRC}" "${INC}" "${INC_SYS}")
