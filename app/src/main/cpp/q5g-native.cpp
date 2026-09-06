#include "fo3-chair-render-mesh.h"

#include <GLES3/gl3.h>
#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr const char* Q5G_TAG = "FalloutQuest";
constexpr float FO3_UNITS_PER_METRE = 70.0f;
constexpr float FLOOR_Y = -1.55f;
constexpr float CHAIR_DISTANCE_METRES = -2.20f;

#define Q5G_LOGI(...) __android_log_print(ANDROID_LOG_INFO, Q5G_TAG, __VA_ARGS__)
#define Q5G_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, Q5G_TAG, __VA_ARGS__)

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

Vec3 Sub(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 Normalize(const Vec3& v) {
    const float length = std::sqrt(std::max(0.0f, Dot(v, v)));
    if (length < 1e-6f) return {0.0f, 1.0f, 0.0f};
    return {v.x / length, v.y / length, v.z / length};
}

GLuint CompileQ5GShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        Q5G_LOGE("Q5G shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint CreateQ5GProgram() {
    static const char* vertexSource = R"(
        #version 300 es
        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in float aShade;
        uniform mat4 uMvp;
        out float vShade;
        void main() {
            vShade = aShade;
            gl_Position = uMvp * vec4(aPosition, 1.0);
        }
    )";

    static const char* fragmentSource = R"(
        #version 300 es
        precision mediump float;
        in float vShade;
        uniform vec3 uColor;
        out vec4 fragColor;
        void main() {
            fragColor = vec4(uColor * vShade, 1.0);
        }
    )";

    GLuint vs = CompileQ5GShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = CompileQ5GShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vs || !fs) return 0;

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[1024]{};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        Q5G_LOGE("Q5G shader link failed: %s", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

GLuint gChairProgram = 0;
GLuint gChairVao = 0;
GLuint gChairVbo = 0;
GLsizei gChairVertexCount = 0;
GLint gChairMvpLocation = -1;
GLint gChairColorLocation = -1;
GLuint gDepthRenderbuffer = 0;
GLsizei gDepthWidth = 0;
GLsizei gDepthHeight = 0;
bool gChairReady = false;
bool gLoggedFirstDraw = false;

bool InitializeChairRenderer() {
    if (gChairReady) return true;

    Fo3StaticMesh mesh;
    if (!LoadMegatonChairStripMesh(mesh) || mesh.positions.empty() || mesh.indices.empty()) {
        Q5G_LOGE("Q5G FAILED: chair mesh was not available for GLES upload");
        return false;
    }

    const size_t vertexCount = mesh.positions.size() / 3u;
    std::vector<Vec3> worldPositions(vertexCount);

    Vec3 minimum{1e30f, 1e30f, 1e30f};
    Vec3 maximum{-1e30f, -1e30f, -1e30f};
    for (size_t i = 0; i < vertexCount; ++i) {
        const float gameX = mesh.positions[i * 3u + 0u];
        const float gameY = mesh.positions[i * 3u + 1u];
        const float gameZ = mesh.positions[i * 3u + 2u];

        Vec3 p{
            gameX / FO3_UNITS_PER_METRE,
            gameZ / FO3_UNITS_PER_METRE,
            -gameY / FO3_UNITS_PER_METRE,
        };
        worldPositions[i] = p;
        minimum.x = std::min(minimum.x, p.x);
        minimum.y = std::min(minimum.y, p.y);
        minimum.z = std::min(minimum.z, p.z);
        maximum.x = std::max(maximum.x, p.x);
        maximum.y = std::max(maximum.y, p.y);
        maximum.z = std::max(maximum.z, p.z);
    }

    const float centerX = (minimum.x + maximum.x) * 0.5f;
    const float centerZ = (minimum.z + maximum.z) * 0.5f;
    const Vec3 offset{
        -centerX,
        FLOOR_Y - minimum.y,
        CHAIR_DISTANCE_METRES - centerZ,
    };
    for (Vec3& p : worldPositions) {
        p.x += offset.x;
        p.y += offset.y;
        p.z += offset.z;
    }

    const Vec3 lightDirection = Normalize({0.35f, 0.85f, 0.40f});
    std::vector<float> expanded;
    expanded.reserve(mesh.indices.size() * 4u);

    for (size_t i = 0; i + 2u < mesh.indices.size(); i += 3u) {
        const uint32_t ia = mesh.indices[i + 0u];
        const uint32_t ib = mesh.indices[i + 1u];
        const uint32_t ic = mesh.indices[i + 2u];
        if (ia >= worldPositions.size() || ib >= worldPositions.size() ||
            ic >= worldPositions.size()) {
            Q5G_LOGE("Q5G FAILED: decoded triangle index exceeded vertex count");
            return false;
        }

        const Vec3& a = worldPositions[ia];
        const Vec3& b = worldPositions[ib];
        const Vec3& c = worldPositions[ic];
        const Vec3 normal = Normalize(Cross(Sub(b, a), Sub(c, a)));
        const float shade = 0.30f + 0.70f * std::fabs(Dot(normal, lightDirection));

        const Vec3 triangle[3]{a, b, c};
        for (const Vec3& p : triangle) {
            expanded.push_back(p.x);
            expanded.push_back(p.y);
            expanded.push_back(p.z);
            expanded.push_back(shade);
        }
    }

    if (expanded.empty()) return false;

    gChairProgram = CreateQ5GProgram();
    if (!gChairProgram) return false;
    gChairMvpLocation = glGetUniformLocation(gChairProgram, "uMvp");
    gChairColorLocation = glGetUniformLocation(gChairProgram, "uColor");

    glGenVertexArrays(1, &gChairVao);
    glBindVertexArray(gChairVao);
    glGenBuffers(1, &gChairVbo);
    glBindBuffer(GL_ARRAY_BUFFER, gChairVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(expanded.size() * sizeof(float)),
                 expanded.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    gChairVertexCount = static_cast<GLsizei>(expanded.size() / 4u);
    gChairReady = glGetError() == GL_NO_ERROR;

    const float width = maximum.x - minimum.x;
    const float height = maximum.y - minimum.y;
    const float depth = maximum.z - minimum.z;
    if (gChairReady) {
        Q5G_LOGI("Q5G READY: REAL chair03.nif uploaded to GLES vertices=%d triangles=%d size=%.2fm x %.2fm x %.2fm position=(0, floor %.2f, %.2f)",
                 gChairVertexCount,
                 gChairVertexCount / 3,
                 width, height, depth,
                 FLOOR_Y, CHAIR_DISTANCE_METRES);
    } else {
        Q5G_LOGE("Q5G FAILED: GLES upload returned error");
    }
    return gChairReady;
}

void RenderInjectedChair() {
    if (!gChairReady || !gChairProgram || gChairVertexCount <= 0) return;

    GLint mainProgram = 0;
    GLint mainVao = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &mainProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &mainVao);
    if (mainProgram == 0) return;

    const GLint sourceMvp = glGetUniformLocation(static_cast<GLuint>(mainProgram), "uMvp");
    if (sourceMvp < 0) return;

    GLfloat mvp[16]{};
    glGetUniformfv(static_cast<GLuint>(mainProgram), sourceMvp, mvp);

    glUseProgram(gChairProgram);
    glUniformMatrix4fv(gChairMvpLocation, 1, GL_FALSE, mvp);
    glUniform3f(gChairColorLocation, 0.78f, 0.56f, 0.25f);
    glBindVertexArray(gChairVao);
    glDrawArrays(GL_TRIANGLES, 0, gChairVertexCount);

    glBindVertexArray(static_cast<GLuint>(mainVao));
    glUseProgram(static_cast<GLuint>(mainProgram));

    if (!gLoggedFirstDraw) {
        gLoggedFirstDraw = true;
        Q5G_LOGI("Q5G VISIBLE: real Fallout 3 chair submitted to both-eye OpenXR render path");
    }
}

void Q5GGenFramebuffers(GLsizei n, GLuint* framebuffers) {
    glGenFramebuffers(n, framebuffers);
    InitializeChairRenderer();
}

void Q5GDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
    glDeleteFramebuffers(n, framebuffers);
    if (gChairVbo) glDeleteBuffers(1, &gChairVbo);
    if (gChairVao) glDeleteVertexArrays(1, &gChairVao);
    if (gChairProgram) glDeleteProgram(gChairProgram);
    if (gDepthRenderbuffer) glDeleteRenderbuffers(1, &gDepthRenderbuffer);
    gChairVbo = 0;
    gChairVao = 0;
    gChairProgram = 0;
    gDepthRenderbuffer = 0;
    gChairReady = false;
}

void Q5GViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    glViewport(x, y, width, height);
    if (width <= 0 || height <= 0) return;

    if (!gDepthRenderbuffer) glGenRenderbuffers(1, &gDepthRenderbuffer);
    GLint previousRenderbuffer = 0;
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &previousRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, gDepthRenderbuffer);
    if (gDepthWidth != width || gDepthHeight != height) {
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        gDepthWidth = width;
        gDepthHeight = height;
    }
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, gDepthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(previousRenderbuffer));
}

void Q5GDisable(GLenum cap) {
    if (cap == GL_DEPTH_TEST) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        return;
    }
    glDisable(cap);
}

void Q5GClear(GLbitfield mask) {
    glClear(mask | GL_DEPTH_BUFFER_BIT);
}

void Q5GDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (mode == GL_TRIANGLES && count == 3 && (first == 0 || first == 3)) {
        return;
    }
    if (mode == GL_TRIANGLES && count == 3 && first == 6) {
        RenderInjectedChair();
        return;
    }
    glDrawArrays(mode, first, count);
}

} // namespace

#define glGenFramebuffers Q5GGenFramebuffers
#define glDeleteFramebuffers Q5GDeleteFramebuffers
#define glViewport Q5GViewport
#define glDisable Q5GDisable
#define glClear Q5GClear
#define glDrawArrays Q5GDrawArrays
#include "q4-native.cpp"
#undef glDrawArrays
#undef glClear
#undef glDisable
#undef glViewport
#undef glDeleteFramebuffers
#undef glGenFramebuffers
