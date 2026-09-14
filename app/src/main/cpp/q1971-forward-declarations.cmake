# Q16.25 linkage/order repair.
# q1970 defines its active-draw predicate and native-LOD probe near the mature
# draw helper, while older scene-swap/stream functions appear earlier in the
# generated translation unit. Forward-declare them inside the same anonymous
# renderer namespace without changing ownership or behaviour.

set(Q1971_NATIVE_FILE "${CMAKE_CURRENT_BINARY_DIR}/q6h-native-generated.cpp")
if(NOT EXISTS "${Q1971_NATIVE_FILE}")
    message(FATAL_ERROR "Q16.25 forward repair expected generated renderer")
endif()
file(READ "${Q1971_NATIVE_FILE}" Q1971_NATIVE_SOURCE)

set(Q1971_NAMESPACE_OLD "namespace {\n")
set(Q1971_NAMESPACE_NEW [==[
namespace {
struct GpuObject;
bool Q1970ShouldRenderFullDetail(const GpuObject& object);
void Q1970ProbeNativeLod(float gameX, float gameY);
]==])
string(FIND "${Q1971_NATIVE_SOURCE}" "${Q1971_NAMESPACE_OLD}" Q1971_NAMESPACE_POS)
if(Q1971_NAMESPACE_POS EQUAL -1)
    message(FATAL_ERROR "Q16.25 forward repair could not find renderer namespace")
endif()
string(REPLACE "${Q1971_NAMESPACE_OLD}" "${Q1971_NAMESPACE_NEW}"
       Q1971_NATIVE_SOURCE "${Q1971_NATIVE_SOURCE}")

string(FIND "${Q1971_NATIVE_SOURCE}"
       "bool Q1970ShouldRenderFullDetail(const GpuObject& object);"
       Q1971_DRAW_DECL_OK)
string(FIND "${Q1971_NATIVE_SOURCE}"
       "void Q1970ProbeNativeLod(float gameX, float gameY);"
       Q1971_LOD_DECL_OK)
if(Q1971_DRAW_DECL_OK EQUAL -1 OR Q1971_LOD_DECL_OK EQUAL -1)
    message(FATAL_ERROR "Q16.25 generated forward declaration verification failed")
endif()

file(WRITE "${Q1971_NATIVE_FILE}" "${Q1971_NATIVE_SOURCE}")
message(STATUS "Q16.25 generated renderer forward declarations enabled")
