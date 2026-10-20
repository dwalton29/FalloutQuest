#pragma once
// Frozen v237 LAND programs for same-build terrain A/B.
// Kept bit-for-bit identical to tests/render/fixtures/terrain_v237.{vert,frag}.
namespace fo3terrain {
inline constexpr const char* kV237VertexShader = R"FQ239(#version 300 es
        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in vec3 aNormal;
        layout(location = 2) in vec2 aTexCoord;
        layout(location = 3) in float aAlpha;
        layout(location = 4) in vec3 aVertexColor;
        uniform mat4 uMvp;
        uniform highp float uFogNearVertexQ1532;
        uniform highp float uFogFarVertexQ1532;
        uniform highp float uFogPowerVertexQ1532;
        out vec3 vNormal;
        out highp float vFogFactorQ1532;
        out vec2 vTexCoord;
        out float vAlpha;
        out vec3 vVertexColor;
        out vec3 vPosition;
        out vec4 vShadowCoord;
        uniform mat4 uLightMvp;
        void main() {
            vNormal = aNormal;
            vTexCoord = aTexCoord;
            vAlpha = aAlpha;
            vVertexColor = aVertexColor;
            vPosition = aPosition;
            vShadowCoord = uLightMvp * vec4(aPosition, 1.0);
            vec4 q1532Clip = uMvp * vec4(aPosition, 1.0);
            gl_Position = q1532Clip;
            float q1532FogDistance = length(q1532Clip.xyz);
            float q1532FogT = 1.0 - clamp(
                (uFogFarVertexQ1532 - q1532FogDistance) /
                (uFogFarVertexQ1532 - uFogNearVertexQ1532), 0.0, 1.0);
            vFogFactorQ1532 = pow(q1532FogT, uFogPowerVertexQ1532);
        }
    )FQ239";
inline constexpr const char* kV237FragmentShader = R"FQ239(#version 300 es
        precision mediump float;
        in vec3 vNormal;
        in vec2 vTexCoord;
        in float vAlpha;
        in vec3 vVertexColor;
        in vec3 vPosition;
        in highp float vFogFactorQ1532;
        in vec4 vShadowCoord;
        uniform sampler2D uDiffuse;
        uniform sampler2D uNormalGloss;
        uniform int uUseNormal;
        uniform float uTerrainSpecularEnabled;
        uniform int uUseTexture;
        uniform vec3 uAmbientColor;
        uniform vec3 uSunlightColor;
        uniform vec3 uSunDirection;
        uniform vec3 uEyePosition;
        uniform vec3 uFogColor;
        uniform float uFogNear;
        uniform float uFogFar;
        uniform float uFogPower;
        uniform int uLocalLightCount;
        uniform vec4 uLocalLightPosRadius[8];
        uniform vec4 uLocalLightColorFalloff[8];
        uniform sampler2D uShadowMap;
        uniform vec2 uShadowTexelSize;
        uniform float uShadowsEnabled;
        uniform float uWaterReflectionClipEnabledQ2090;
        uniform float uWaterReflectionPlaneYQ2090;
        out vec4 fragColor;
        float Q1050ShadowVisibility(vec4 shadowCoord, vec3 N, vec3 L) {
            if (uShadowsEnabled < 0.5 || shadowCoord.w <= 0.0) return 1.0;
            vec3 projected = shadowCoord.xyz / shadowCoord.w;
            projected = projected * 0.5 + 0.5;
            if (projected.x <= 0.0 || projected.x >= 1.0 ||
                projected.y <= 0.0 || projected.y >= 1.0 ||
                projected.z <= 0.0 || projected.z >= 1.0) return 1.0;
            float ndotl = max(dot(N, L), 0.0);
            float bias = max(0.00015, 0.00065 * (1.0 - ndotl));
            vec2 halfTexel = uShadowTexelSize * 0.5;
            float visible = 0.0;
            visible += projected.z - bias <= texture(uShadowMap, projected.xy + vec2(-halfTexel.x, -halfTexel.y)).r ? 1.0 : 0.0;
            visible += projected.z - bias <= texture(uShadowMap, projected.xy + vec2( halfTexel.x, -halfTexel.y)).r ? 1.0 : 0.0;
            visible += projected.z - bias <= texture(uShadowMap, projected.xy + vec2(-halfTexel.x,  halfTexel.y)).r ? 1.0 : 0.0;
            visible += projected.z - bias <= texture(uShadowMap, projected.xy + vec2( halfTexel.x,  halfTexel.y)).r ? 1.0 : 0.0;
            return visible * 0.25;
        }
        void main() {
            if (uWaterReflectionClipEnabledQ2090 > 0.5 &&
                vPosition.y < uWaterReflectionPlaneYQ2090) {
                discard;
            }
            vec3 N = normalize(vNormal);
            vec4 normalGloss = vec4(0.5, 0.5, 1.0, 0.0);
            if (uUseNormal != 0) {
                normalGloss = texture(uNormalGloss, vTexCoord);
                vec3 tangentNormal = normalize(normalGloss.xyz * 2.0 - 1.0);
                vec3 authoredU = vec3(1.0, 0.0, 0.0);
                vec3 T = authoredU - N * dot(N, authoredU);
                if (dot(T, T) < 0.000001) T = vec3(0.0, 0.0, -1.0);
                T = normalize(T);
                // Increasing LAND V follows game +Y, which is renderer -Z.
                vec3 B = normalize(cross(N, T));
                N = normalize(mat3(T, B, N) * tangentNormal);
            }

            vec3 lightDirection = normalize(uSunDirection);
            float lambert = max(dot(N, lightDirection), 0.0);
            vec3 earthFallback = vec3(0.34, 0.27, 0.18);
            vec3 albedo = (uUseTexture != 0 ? texture(uDiffuse, vTexCoord).rgb : earthFallback) * vVertexColor;
            float q1050SunVisibility = Q1050ShadowVisibility(vShadowCoord, N, lightDirection);
            vec3 q1570LandWorldDiffuse =
                albedo * (uAmbientColor + uSunlightColor * lambert * q1050SunVisibility);
            vec3 lit = q1570LandWorldDiffuse;
            if (uUseNormal != 0 && uTerrainSpecularEnabled > 0.5) {
                vec3 V = normalize(uEyePosition - vPosition);
                vec3 H = normalize(lightDirection + V);
                float specular = pow(max(dot(N, H), 0.0), 18.0) * normalGloss.a * 0.24;
                lit += uSunlightColor * specular * q1050SunVisibility;
            }
            for (int i = 0; i < 8; ++i) {
                if (i >= uLocalLightCount) break;
                vec3 toLight = uLocalLightPosRadius[i].xyz - vPosition;
                float distanceToLight = length(toLight);
                float radius = max(uLocalLightPosRadius[i].w, 0.001);
                if (distanceToLight >= radius) continue;
                vec3 L = toLight / max(distanceToLight, 0.001);
                float localLambert = max(dot(N, L), 0.0);
                float edge = clamp(1.0 - distanceToLight / radius, 0.0, 1.0);
                float attenuation = pow(edge, max(uLocalLightColorFalloff[i].w, 0.25));
                lit += albedo * uLocalLightColorFalloff[i].rgb * localLambert * attenuation;
            }
            // PC SLS2100.vso computes LAND fog per vertex from transformed
            // position, FogParam=(far, far-near, power, _), then SLS2124.pso
            // blends against the interpolated factor. Use the already matching
            // vertex result instead of recomputing a different world-distance fog.
            float fogFactor = vFogFactorQ1532;
            lit = mix(lit, uFogColor, fogFactor);
            fragColor = vec4(max(lit, vec3(0.0)), clamp(vAlpha, 0.0, 1.0));
        }
    )FQ239";
} // namespace fo3terrain
