# Q16.26 configure-anchor repair.
#
# The first q1980 CI pass proved every earlier Q16.26 transform landed until the
# collision phase-order hook. The live mature renderer still has exactly one
# bare `phase = Terrain` transition for Q1900AdvanceCollision, but the compound
# q1980 anchor also assumed the immediately following log formatting. Earlier
# generated-source patches have changed that surrounding text. Build a corrected
# generated copy of q1980 using the unique phase line only, then execute it.

set(Q1981_Q1980_SOURCE_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/q1980-bounded-residency-collision-first.cmake")
if(NOT EXISTS "${Q1981_Q1980_SOURCE_FILE}")
    message(FATAL_ERROR "Q16.26 anchor repair expected q1980 source")
endif()
file(READ "${Q1981_Q1980_SOURCE_FILE}" Q1981_Q1980_SOURCE)

# Use a wider bracket delimiter because the literal q1980 source below contains
# its own [==[ ... ]==] bracket arguments.
set(Q1981_BRITTLE_BLOCK [====[
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

set(Q1981_ROBUST_BLOCK [====[
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

string(FIND "${Q1981_Q1980_SOURCE}" "${Q1981_BRITTLE_BLOCK}" Q1981_BRITTLE_POS)
if(Q1981_BRITTLE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.26 anchor repair could not find q1980 brittle collision block")
endif()
string(REPLACE "${Q1981_BRITTLE_BLOCK}" "${Q1981_ROBUST_BLOCK}"
       Q1981_Q1980_SOURCE "${Q1981_Q1980_SOURCE}")

set(Q1981_GENERATED_Q1980
    "${CMAKE_CURRENT_BINARY_DIR}/q1980-bounded-residency-collision-first-fixed.cmake")
file(WRITE "${Q1981_GENERATED_Q1980}" "${Q1981_Q1980_SOURCE}")
include("${Q1981_GENERATED_Q1980}")
