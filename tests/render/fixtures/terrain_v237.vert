#version 300 es
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
    