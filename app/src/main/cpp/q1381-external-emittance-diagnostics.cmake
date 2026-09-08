# Q13.8 diagnostic hardening: always build the Megaton XEMI cache before GPU
# upload, even if no currently-rendered NIF shape exposes the external-emittance
# flag. This makes a zero-match result observable instead of ambiguous.

set(Q1381_OLD_UPLOAD [==[
    gObjects.reserve(selected.size());
]==])
set(Q1381_NEW_UPLOAD [==[
    LoadFo3ExternalEmittanceQ1380(0x00000A74u);
    gObjects.reserve(selected.size());
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1381_OLD_UPLOAD}" Q1381_UPLOAD_POS)
if(Q1381_UPLOAD_POS EQUAL -1)
    message(FATAL_ERROR "Q13.8 diagnostics could not find GPU upload entry")
endif()
string(REPLACE "${Q1381_OLD_UPLOAD}" "${Q1381_NEW_UPLOAD}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

set(Q1381_OLD_READY [==[
    gSceneReady = gObjects.size() >= 2u;
]==])
set(Q1381_NEW_READY [==[
    Q6H_LOGI("Q13.8 EMITTANCE GPU: nifFlagShapes=%zu resolvedShapes=%zu fixedLIGHShapes=%zu regionDayShapes=%zu shaderFlag=0x%08X dayEndpoint=1 authoredOnly=1 effectsFolderStillDeferred=1",
             gQ1380ExternalFlagShapes, gQ1380ExternalResolvedShapes,
             gQ1380ExternalFixedShapes, gQ1380ExternalRegionShapes,
             fo3emittanceq1380::EXTERNAL_EMITTANCE_SHADER_FLAG);
    gSceneReady = gObjects.size() >= 2u;
]==])
string(FIND "${Q6H_NATIVE_SOURCE}" "${Q1381_OLD_READY}" Q1381_READY_POS)
if(Q1381_READY_POS EQUAL -1)
    message(FATAL_ERROR "Q13.8 diagnostics could not find scene-ready assignment")
endif()
string(REPLACE "${Q1381_OLD_READY}" "${Q1381_NEW_READY}"
       Q6H_NATIVE_SOURCE "${Q6H_NATIVE_SOURCE}")

string(FIND "${Q6H_NATIVE_SOURCE}" "LoadFo3ExternalEmittanceQ1380(0x00000A74u)" Q1381_LOAD_OK)
string(FIND "${Q6H_NATIVE_SOURCE}" "Q13.8 EMITTANCE GPU" Q1381_LOG_OK)
if(Q1381_LOAD_OK EQUAL -1 OR Q1381_LOG_OK EQUAL -1)
    message(FATAL_ERROR "Q13.8 unconditional diagnostics verification failed")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp" "${Q6H_NATIVE_SOURCE}")
message(STATUS "Q13.8 external-emittance diagnostics made unconditional")
