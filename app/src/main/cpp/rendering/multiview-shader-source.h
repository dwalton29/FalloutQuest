#pragma once
#include <string>
namespace questrender {
inline void TrimShaderPreamble(std::string& text) {
    const auto first=text.find_first_not_of(" \t\r\n");
    if(first!=std::string::npos) text.erase(0,first);
}
inline bool ReplaceShaderToken(std::string& text, const std::string& from, const std::string& to) {
    const auto at=text.find(from);
    if(at==std::string::npos || text.find(from,at+from.size())!=std::string::npos) return false;
    text.replace(at,from.size(),to); return true;
}
// Separate variants preserve monoview shaders for mirrors and driver fallback.
inline bool WorldMultiviewSources(std::string& vertex,std::string& fragment) {
    TrimShaderPreamble(vertex);TrimShaderPreamble(fragment);
    return ReplaceShaderToken(vertex,"#version 300 es","#version 300 es\n#extension GL_OVR_multiview2 : require\nlayout(num_views=2) in;\nflat out highp uint vStereoView;\n") &&
        ReplaceShaderToken(vertex,"uniform mat4 uMvp;","uniform mat4 uStereoMvp[2];\n#define uMvp uStereoMvp[gl_ViewID_OVR]\n") &&
        ReplaceShaderToken(vertex,"uniform highp vec3 uEyePositionVertexQ1630;","uniform highp vec3 uStereoEye[2];\n#define uEyePositionVertexQ1630 uStereoEye[gl_ViewID_OVR]\n") &&
        ReplaceShaderToken(vertex,"void main() {","void main() {\nvStereoView=gl_ViewID_OVR;\n") &&
        ReplaceShaderToken(fragment,"precision mediump float;","precision mediump float;\nflat in highp uint vStereoView;\n") &&
        ReplaceShaderToken(fragment,"uniform vec3 uEyePosition;","uniform highp vec3 uStereoEye[2];\n#define uEyePosition uStereoEye[vStereoView]\n");
}
}
