# Q16.26 configure-anchor repair.
#
# Q16.26 intentionally patches a mature generated renderer assembled by many
# earlier milestones. Keep q1980's runtime design frozen; this wrapper only
# relaxes two phase-order hooks whose surrounding diagnostic text has drifted.

set(Q1981_Q1980_SOURCE_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/q1980-bounded-residency-collision-first.cmake")
if(NOT EXISTS "${Q1981_Q1980_SOURCE_FILE}")
    message(FATAL_ERROR "Q16.26 anchor repair expected q1980 source")
endif()
file(READ "${Q1981_Q1980_SOURCE_FILE}" Q1981_Q1980_SOURCE)

# Use wider bracket delimiters because the literal q1980 source contains its own
# [==[ ... ]==] bracket arguments.
set(Q1981_COLLISION_BRITTLE [====[
# Collision now returns to visual CPU staging rather than jumping straight to LAND.
set(Q1980_COLLISION_NEXT_OLD [==[
    gPendingStreamQ1900.phase = Q1900StreamPhase::Terrain;

    Q6H_LOGI("Q16.25 COLLISION READY:
]==])
set(Q1980_COLLISION_NEXT_NEW [==[
    gPendingStreamQ1900.phase = gPendingStreamQ1900.incomingPlacements.empty()
        ? Q1900StreamPhase::Terrain : Q1900StreamPhase::Cpu;

    Q6H_LOGI("Q16.25 COLLISION READY:
]==])
string(FIND "${Q1980_NATIVE_SOURCE}" "${Q1980_COLLISION_NEXT_OLD}" Q1980_COLLISION_NEXT_POS)
if(Q1980_COLLISION_NEXT_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 could not find collision -> terrain phase transition")
endif()
string(REPLACE "${Q1980_COLLISION_NEXT_OLD}" "${Q1980_COLLISION_NEXT_NEW}"
       Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")
]====])

set(Q1981_COLLISION_ROBUST [====[
# Collision now returns to visual CPU staging rather than jumping straight to LAND.
# The bare transition is unique in the mature source at this point; do not couple
# the hook to diagnostic whitespace/order from earlier generated-source layers.
set(Q1980_COLLISION_NEXT_OLD
    "    gPendingStreamQ1900.phase = Q1900StreamPhase::Terrain;")
set(Q1980_COLLISION_NEXT_NEW [==[
    gPendingStreamQ1900.phase = gPendingStreamQ1900.incomingPlacements.empty()
        ? Q1900StreamPhase::Terrain : Q1900StreamPhase::Cpu;
]==])
string(FIND "${Q1980_NATIVE_SOURCE}" "${Q1980_COLLISION_NEXT_OLD}" Q1980_COLLISION_NEXT_POS)
if(Q1980_COLLISION_NEXT_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 could not find unique collision -> terrain phase transition")
endif()
string(REPLACE "${Q1980_COLLISION_NEXT_OLD}" "${Q1980_COLLISION_NEXT_NEW}"
       Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")
]====])

string(FIND "${Q1981_Q1980_SOURCE}" "${Q1981_COLLISION_BRITTLE}" Q1981_COLLISION_POS)
if(Q1981_COLLISION_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 anchor repair could not find q1980 brittle collision block")
endif()
string(REPLACE "${Q1981_COLLISION_BRITTLE}" "${Q1981_COLLISION_ROBUST}"
       Q1981_Q1980_SOURCE "${Q1981_Q1980_SOURCE}")

# The second failed CI reached the analogous GPU transition. Again, the phase
# line itself is stable/unique; only the adjacent log prefix has drifted.
set(Q1981_GPU_BRITTLE [====[
set(Q1980_GPU_NEXT_OLD [==[
        gPendingStreamQ1900.phase = Q1900StreamPhase::Collision;
        Q6H_LOGI("Q16.25 GPU READY:
]==])
set(Q1980_GPU_NEXT_NEW [==[
        gPendingStreamQ1900.phase = Q1900StreamPhase::Terrain;
        Q6H_LOGI("Q16.25 GPU READY:
]==])
string(FIND "${Q1980_NATIVE_SOURCE}" "${Q1980_GPU_NEXT_OLD}" Q1980_GPU_NEXT_POS)
if(Q1980_GPU_NEXT_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 could not find GPU -> collision phase transition")
endif()
string(REPLACE "${Q1980_GPU_NEXT_OLD}" "${Q1980_GPU_NEXT_NEW}"
       Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")
]====])

set(Q1981_GPU_ROBUST [====[
set(Q1980_GPU_NEXT_OLD
    "        gPendingStreamQ1900.phase = Q1900StreamPhase::Collision;")
set(Q1980_GPU_NEXT_NEW
    "        gPendingStreamQ1900.phase = Q1900StreamPhase::Terrain;")
string(FIND "${Q1980_NATIVE_SOURCE}" "${Q1980_GPU_NEXT_OLD}" Q1980_GPU_NEXT_POS)
if(Q1980_GPU_NEXT_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 could not find unique GPU -> collision phase transition")
endif()
string(REPLACE "${Q1980_GPU_NEXT_OLD}" "${Q1980_GPU_NEXT_NEW}"
       Q1980_NATIVE_SOURCE "${Q1980_NATIVE_SOURCE}")
]====])

string(FIND "${Q1981_Q1980_SOURCE}" "${Q1981_GPU_BRITTLE}" Q1981_GPU_POS)
if(Q1981_GPU_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 anchor repair could not find q1980 brittle GPU block")
endif()
string(REPLACE "${Q1981_GPU_BRITTLE}" "${Q1981_GPU_ROBUST}"
       Q1981_Q1980_SOURCE "${Q1981_Q1980_SOURCE}")

set(Q1981_GENERATED_Q1980
    "${CMAKE_CURRENT_BINARY_DIR}/q1980-bounded-residency-collision-first-fixed.cmake")
file(WRITE "${Q1981_GENERATED_Q1980}" "${Q1981_Q1980_SOURCE}")
include("${Q1981_GENERATED_Q1980}")

# Q16.27 runs only after the repaired Q16.26 generated sources exist.
include("${CMAKE_CURRENT_SOURCE_DIR}/q1990-persistent-collision-native-lod.cmake")
