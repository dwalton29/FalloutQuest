# Q11.3: keep Fallout 3/Gamebryo UVs in the same vertical convention as the
# raw DDS rows uploaded by LoadFalloutTextureRgba.
#
# DDS data is decoded in file/D3D row order and uploaded without a CPU row flip.
# GLES interprets the first uploaded row at the opposite texture origin, which
# already compensates for the D3D-vs-GL V-axis convention. Q6A/Q6H then flipped
# the NIF V coordinate a second time, vertically mirroring every static texture.
# Atlas-style Megaton scrap/fuselage/sign textures are especially sensitive: a
# double flip makes UV islands sample the wrong region of the atlas even though
# the NIF and DDS path are both correct.
#
# Remove that extra V flip and, at the same time, remove the tangent-space Y
# inversion that existed solely to compensate for it.

set(Q1130_OLD_V [==[
        const float v = 1.0f - cpu.mesh.texcoords[static_cast<size_t>(index) * 2u + 1u];
]==])
set(Q1130_NEW_V [==[
        const float v = cpu.mesh.texcoords[static_cast<size_t>(index) * 2u + 1u];
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1130_OLD_V}" Q1130_V_POS)
if(Q1130_V_POS EQUAL -1)
    message(FATAL_ERROR "Q11.3 could not find static NIF V-flip hook")
endif()
string(REPLACE "${Q1130_OLD_V}" "${Q1130_NEW_V}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1130_OLD_NORMAL_Y [==[
            tangentNormal.y = -tangentNormal.y;
]==])
set(Q1130_NEW_NORMAL_Y [==[
            // Q11.3: no tangent-space Y inversion; UVs are no longer double-flipped.
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1130_OLD_NORMAL_Y}" Q1130_NORMAL_POS)
if(Q1130_NORMAL_POS EQUAL -1)
    message(FATAL_ERROR "Q11.3 could not find tangent-space Y inversion hook")
endif()
string(REPLACE "${Q1130_OLD_NORMAL_Y}" "${Q1130_NEW_NORMAL_Y}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" "const float v = cpu.mesh.texcoords" Q1130_V_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q11.3: no tangent-space Y inversion" Q1130_N_OK)
if(Q1130_V_OK EQUAL -1 OR Q1130_N_OK EQUAL -1)
    message(FATAL_ERROR "Q11.3 static UV orientation verification failed")
endif()

# Q11.3 is now the final renderer mutation.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q11.3 static DDS/Gamebryo UV orientation fixed")
