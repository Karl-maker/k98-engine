#include "OpenGLRenderSystem.hpp"

#include "../core/assets/ModelAsset.hpp"
#include "../ecs/Entity.hpp"
#include "../ecs/Registry.hpp"
#include "../math/Mat4.hpp"
#include "../math/Vertex.hpp"
#include "../components/BoneInstanceComponent.hpp"
#include "../components/CameraComponent.hpp"
#include "../components/GameplayTags.hpp"
#include "../components/GpuSkinPaletteComponent.hpp"
#include "../components/SocketComponent.hpp"
#include "../components/StaticMeshComponent.hpp"
#include "../components/TransformComponent.hpp"
#include "../components/HdriEnvironmentComponent.hpp"
#include "../components/LightingComponent.hpp"
#include "../components/WorldTransformComponent.hpp"
#include "../math/Vec3.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#define GLEW_STATIC
#include <GL/glew.h>
#endif

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "stb_image.h"
#include <webp/decode.h>

namespace {

std::string hdriPathLowerExt(const std::string& path) {
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return {};
    std::string ext = path.substr(dot);
    for (char& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

std::string resolveHdriFilesystemPath(const std::string& path) {
    if (path.empty())
        return {};
    {
        std::ifstream test(path, std::ios::binary);
        if (test.good())
            return path;
    }
#ifdef GAME_ENGINE_PROJECT_ROOT
    std::string root = GAME_ENGINE_PROJECT_ROOT;
    if (!root.empty() && root.back() != '/')
        root += '/';
    const std::string combined = root + path;
    {
        std::ifstream test(combined, std::ios::binary);
        if (test.good())
            return combined;
    }
#endif
    return path;
}

bool readFileAllBytes(const std::string& path, std::vector<unsigned char>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f)
        return false;
    const std::streamsize sz = f.tellg();
    if (sz <= 0)
        return false;
    f.seekg(0);
    out.resize(static_cast<size_t>(sz));
    return static_cast<bool>(f.read(reinterpret_cast<char*>(out.data()), sz));
}

GLuint uploadRgbaEquirect(unsigned char* rgba, int w, int h) {
    if (!rgba || w <= 0 || h <= 0)
        return 0;
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

/// Loads equirectangular RGBA; supports .webp (libwebp) and formats stb_image handles (png, jpg, …).
GLuint loadEquirectTextureFromFile(const std::string& pathIn, std::string& outResolved, std::string& errOut) {
    errOut.clear();
    outResolved = resolveHdriFilesystemPath(pathIn);
    std::vector<unsigned char> bytes;
    if (!readFileAllBytes(outResolved, bytes)) {
        errOut = "HDRI: could not read file: " + outResolved;
        return 0;
    }
    const std::string ext = hdriPathLowerExt(outResolved);
    int w = 0;
    int h = 0;
    unsigned char* rgba = nullptr;

    if (ext == ".webp") {
        rgba = WebPDecodeRGBA(bytes.data(), bytes.size(), &w, &h);
        if (!rgba) {
            errOut = "HDRI: WebP decode failed: " + outResolved;
            return 0;
        }
        GLuint t = uploadRgbaEquirect(rgba, w, h);
        WebPFree(rgba);
        if (!t)
            errOut = "HDRI: OpenGL upload failed: " + outResolved;
        return t;
    }

    int n = 0;
    rgba = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &n, 4);
    if (!rgba) {
        errOut = std::string("HDRI: stbi_load failed (") + (stbi_failure_reason() ? stbi_failure_reason() : "?") + "): " +
            outResolved;
        return 0;
    }
    GLuint t = uploadRgbaEquirect(rgba, w, h);
    stbi_image_free(rgba);
    if (!t)
        errOut = "HDRI: OpenGL upload failed: " + outResolved;
    return t;
}

} // namespace

static const char* kVertSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
uniform mat4 uModel;
uniform mat4 uMVP;
out vec3 vNormal;
void main() {
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* kFragSrc = R"(
#version 330 core
in vec3 vNormal;
uniform vec3 uColor;
uniform vec3 uLightDir;
uniform vec3 uAmbient;
out vec4 FragColor;
void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir);
    float ndl = max(dot(N, L), 0.0);
    vec3 c = uColor * (uAmbient + vec3(0.55) * ndl);
    FragColor = vec4(c, 1.0);
}
)";

static const char* kTexVertSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec4 aTangent;
uniform mat4 uModel;
uniform mat4 uMVP;
out vec3 vWorldPos;
out vec3 vN;
out vec4 vTan;
out vec2 vUV;
void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    mat3 M = mat3(transpose(inverse(uModel)));
    vN = normalize(M * aNormal);
    vTan = vec4(normalize(M * aTangent.xyz), aTangent.w);
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* kTexFragSrc = R"(
#version 330 core
in vec3 vWorldPos;
in vec3 vN;
in vec4 vTan;
in vec2 vUV;
uniform sampler2D uAlbedo;
uniform sampler2D uNormalMap;
uniform sampler2D uOcclusion;
uniform sampler2D uMetallicRoughness;
uniform int uUseNormalMap;
uniform int uUseOcclusion;
uniform int uUseMetallicRoughness;
uniform vec3 uAmbient;
uniform vec3 uCameraPos;
uniform vec3 uTint;
uniform int uLightCount;
#define MAX_LIGHTS 8
uniform vec4 uLightPosType[MAX_LIGHTS];
uniform vec4 uLightDirRange[MAX_LIGHTS];
uniform vec4 uLightColorInt[MAX_LIGHTS];
uniform vec4 uLightSpot[MAX_LIGHTS];
uniform vec4 uLightAttenSpec[MAX_LIGHTS];
uniform vec2 uRim;
uniform sampler2D uEnvMap;
uniform int uUseHdri;
uniform float uHdriIntensity;
uniform float uHdriRotY;
uniform float uHdriDiffuseW;
uniform float uHdriSpecW;
out vec4 FragColor;
void main() {
    vec4 tex = texture(uAlbedo, vUV);
    float a = tex.a < 0.04f ? 1.0f : tex.a;
    vec4 base = vec4(tex.rgb, a) * vec4(uTint, 1.0);
    if (base.a < 0.35)
        discard;
    vec3 N = normalize(vN);
    if (uUseNormalMap != 0) {
        vec3 nm = texture(uNormalMap, vUV).xyz * 2.0 - 1.0;
        vec3 T = normalize(vTan.xyz);
        vec3 B = normalize(cross(N, T) * vTan.w);
        mat3 TBN = mat3(T, B, N);
        N = normalize(TBN * nm);
    }
    float occ = 1.0;
    if (uUseOcclusion != 0)
        occ = texture(uOcclusion, vUV).r;
    float rough = 0.5;
    if (uUseMetallicRoughness != 0)
        rough = texture(uMetallicRoughness, vUV).g;

    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 diffAccum = vec3(0.0);
    vec3 specAccum = vec3(0.0);
    int lc = min(uLightCount, MAX_LIGHTS);
    for (int i = 0; i < lc; ++i) {
        int tp = int(uLightPosType[i].w + 0.5);
        vec3 lcCol = uLightColorInt[i].rgb * uLightColorInt[i].a;
        float cAt = uLightAttenSpec[i].x;
        float specPow = uLightAttenSpec[i].y;
        float specInt = uLightAttenSpec[i].z;
        float halfL = uLightAttenSpec[i].w;
        float lin = uLightSpot[i].z;
        float quad = uLightSpot[i].w;
        float innerC = uLightSpot[i].x;
        float outerC = uLightSpot[i].y;

        vec3 forward = uLightDirRange[i].xyz;
        if (dot(forward, forward) > 1e-8)
            forward = normalize(forward);
        else
            forward = vec3(0.0, -1.0, 0.0);
        float range = uLightDirRange[i].w;

        vec3 L;
        float att = 1.0;
        float spotF = 1.0;

        if (tp == 0) {
            L = normalize(-forward);
        } else {
            vec3 lp = uLightPosType[i].xyz;
            vec3 toL = lp - vWorldPos;
            float dist = length(toL);
            L = toL / max(dist, 1e-4);
            att = 1.0 / (cAt + lin * dist + quad * dist * dist);
            att *= clamp(1.0 - dist / max(range, 1e-2), 0.0, 1.0);
            if (tp == 2) {
                vec3 fromLight = -L;
                float cang = dot(fromLight, forward);
                spotF = smoothstep(outerC, innerC, cang);
            }
        }

        float ndl = max(dot(N, L), 0.0);
        if (halfL > 0.5)
            ndl = ndl * 0.5 + 0.5;
        float diffTerm = mix(0.45, 0.65, rough);
        diffAccum += lcCol * (ndl * diffTerm) * att * spotF;

        vec3 H = normalize(L + V);
        float ndh = max(dot(N, H), 0.0);
        float spec = pow(ndh, specPow) * specInt * (1.0 - rough * 0.65);
        specAccum += lcCol * spec * att * spotF;
    }

    vec3 ambLit = uAmbient * occ;
    vec3 rimCol = vec3(0.0);
    if (uRim.y > 0.001) {
        float rim = pow(1.0 - max(dot(N, V), 0.0), max(uRim.x, 0.5));
        rimCol = base.rgb * rim * uRim.y;
    }
    vec3 c = base.rgb * ambLit + base.rgb * diffAccum + specAccum + rimCol;
    if (uUseHdri != 0) {
        float cR = cos(uHdriRotY);
        float sR = sin(uHdriRotY);
        vec3 envDirDiff = N;
        float dx = cR * envDirDiff.x - sR * envDirDiff.z;
        float dz = sR * envDirDiff.x + cR * envDirDiff.z;
        vec3 dDiff = normalize(vec3(dx, envDirDiff.y, dz));
        float uD = atan(dDiff.z, dDiff.x) / 6.28318530718 + 0.5;
        float vD = acos(clamp(dDiff.y, -1.0, 1.0)) / 3.14159265;
        vec3 envDiff = texture(uEnvMap, vec2(uD, vD)).rgb;
        vec3 Renv = reflect(-V, N);
        float rx = cR * Renv.x - sR * Renv.z;
        float rz = sR * Renv.x + cR * Renv.z;
        vec3 dSpec = normalize(vec3(rx, Renv.y, rz));
        float uS = atan(dSpec.z, dSpec.x) / 6.28318530718 + 0.5;
        float vS = acos(clamp(dSpec.y, -1.0, 1.0)) / 3.14159265;
        vec3 envSpec = texture(uEnvMap, vec2(uS, vS)).rgb;
        c += base.rgb * envDiff * uHdriIntensity * uHdriDiffuseW * occ;
        c += envSpec * uHdriIntensity * uHdriSpecW * (1.0 - rough * 0.85);
    }
    FragColor = vec4(c, base.a);
}
)";

static const char* kSkinTexVertSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aTangent;
layout(location = 4) in ivec4 aBoneIndex;
layout(location = 5) in vec4 aBoneWeight;

layout(std140) uniform SkinPalette {
    mat4 uJointMatrix[512];
};

uniform mat4 uModel;
uniform mat4 uMVP;

out vec3 vWorldPos;
out vec3 vN;
out vec4 vTan;
out vec2 vUV;

void main() {
    vec4 pos = vec4(aPos, 1.0);
    vec3 n = aNormal;
    vec3 txyz = aTangent.xyz;
    float wsum = aBoneWeight.x + aBoneWeight.y + aBoneWeight.z + aBoneWeight.w;

    vec4 sp = vec4(0.0);
    vec3 sn = vec3(0.0);
    vec3 st = vec3(0.0);

    if (wsum > 1e-6) {
        int bi0 = clamp(aBoneIndex.x, 0, 511);
        int bi1 = clamp(aBoneIndex.y, 0, 511);
        int bi2 = clamp(aBoneIndex.z, 0, 511);
        int bi3 = clamp(aBoneIndex.w, 0, 511);
        sp += uJointMatrix[bi0] * pos * aBoneWeight.x;
        sp += uJointMatrix[bi1] * pos * aBoneWeight.y;
        sp += uJointMatrix[bi2] * pos * aBoneWeight.z;
        sp += uJointMatrix[bi3] * pos * aBoneWeight.w;
        mat3 J0 = mat3(uJointMatrix[bi0]);
        mat3 J1 = mat3(uJointMatrix[bi1]);
        mat3 J2 = mat3(uJointMatrix[bi2]);
        mat3 J3 = mat3(uJointMatrix[bi3]);
        sn += J0 * n * aBoneWeight.x;
        sn += J1 * n * aBoneWeight.y;
        sn += J2 * n * aBoneWeight.z;
        sn += J3 * n * aBoneWeight.w;
        st += J0 * txyz * aBoneWeight.x;
        st += J1 * txyz * aBoneWeight.y;
        st += J2 * txyz * aBoneWeight.z;
        st += J3 * txyz * aBoneWeight.w;
    } else {
        sp = pos;
        sn = n;
        st = txyz;
    }
    sn = normalize(sn);
    st = normalize(st);

    vec4 wp = uModel * sp;
    vWorldPos = wp.xyz;
    mat3 M = mat3(transpose(inverse(uModel)));
    vN = normalize(M * sn);
    vTan = vec4(normalize(M * st), aTangent.w);
    vUV = aUV;
    gl_Position = uMVP * sp;
}
)";

namespace {
constexpr int kMaxSkinJoints = 512;
constexpr GLsizeiptr kSkinUboBytes = kMaxSkinJoints * static_cast<GLsizeiptr>(sizeof(Mat4));
} // namespace

static GLuint loadTexture2DFromFile(const std::string& path, bool srgb) {
    if (path.empty())
        return 0;
    int w = 0, h = 0, n = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!data) {
        std::cerr << "Texture load failed: " << path << "\n";
        return 0;
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    (void)srgb;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    stbi_image_free(data);
    return tex;
}

static GLuint make1x1WhiteTexture() {
    unsigned char px[4] = {255, 255, 255, 255};
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return tex;
}

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error: " << log << "\n";
        glDeleteShader(s);
        return 0;
    }
    return s;
}

bool OpenGLRenderSystem::init(int width, int height, const char* title) {
    if (!glfwInit()) {
        std::cerr << "glfwInit failed\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) {
        std::cerr << "glfwCreateWindow failed\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

#if !defined(__APPLE__)
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "glewInit failed\n";
        shutdown();
        return false;
    }
#endif

    glfwGetFramebufferSize(m_window, &m_fbW, &m_fbH);
    glViewport(0, 0, m_fbW, m_fbH);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    GLuint vs = compileShader(GL_VERTEX_SHADER, kVertSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragSrc);
    if (!vs || !fs)
        return false;

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);
    GLint linked = 0;
    glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!linked) {
        char log[1024];
        glGetProgramInfoLog(m_program, sizeof(log), nullptr, log);
        std::cerr << "Program link: " << log << "\n";
        glDeleteProgram(m_program);
        m_program = 0;
        return false;
    }

    buildPyramidMesh();
    buildTexturedShaderPipeline();
    buildSkinnedTexturedShaderPipeline();

    glGenBuffers(1, &m_skinPaletteUbo);
    glBindBuffer(GL_UNIFORM_BUFFER, m_skinPaletteUbo);
    glBufferData(GL_UNIFORM_BUFFER, kSkinUboBytes, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_skinPaletteUbo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    return true;
}

void OpenGLRenderSystem::buildPyramidMesh() {
    struct V {
        float px, py, pz, nx, ny, nz;
    };
    std::vector<V> v;
    // Unit pyramid: apex +Y, square base in plane y = -0.5 (centered at origin).
    const Vec3 apex{0.0f, 0.5f, 0.0f};
    const Vec3 b0{-0.5f, -0.5f, -0.5f};
    const Vec3 b1{0.5f, -0.5f, -0.5f};
    const Vec3 b2{0.5f, -0.5f, 0.5f};
    const Vec3 b3{-0.5f, -0.5f, 0.5f};

    auto addTri = [&](const Vec3& a, const Vec3& b, const Vec3& c) {
        Vec3 e1{b.x - a.x, b.y - a.y, b.z - a.z};
        Vec3 e2{c.x - a.x, c.y - a.y, c.z - a.z};
        Vec3 n = cross(e1, e2);
        n = normalize(n);
        v.push_back({a.x, a.y, a.z, n.x, n.y, n.z});
        v.push_back({b.x, b.y, b.z, n.x, n.y, n.z});
        v.push_back({c.x, c.y, c.z, n.x, n.y, n.z});
    };

    // Base (outward normal -Y, viewed from below)
    addTri(b0, b1, b2);
    addTri(b0, b2, b3);
    // Four sides (CCW when viewed from outside)
    addTri(apex, b1, b0);
    addTri(apex, b2, b1);
    addTri(apex, b3, b2);
    addTri(apex, b0, b3);

    m_vertexCount = static_cast<unsigned int>(v.size());

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(V), v.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void OpenGLRenderSystem::buildTexturedShaderPipeline() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, kTexVertSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kTexFragSrc);
    if (!vs || !fs)
        return;
    m_texProgram = glCreateProgram();
    glAttachShader(m_texProgram, vs);
    glAttachShader(m_texProgram, fs);
    glLinkProgram(m_texProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint linked = 0;
    glGetProgramiv(m_texProgram, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[1024];
        glGetProgramInfoLog(m_texProgram, sizeof(log), nullptr, log);
        std::cerr << "Textured program link: " << log << "\n";
        glDeleteProgram(m_texProgram);
        m_texProgram = 0;
    }
}

void OpenGLRenderSystem::buildSkinnedTexturedShaderPipeline() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, kSkinTexVertSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kTexFragSrc);
    if (!vs || !fs)
        return;
    m_skinTexProgram = glCreateProgram();
    glAttachShader(m_skinTexProgram, vs);
    glAttachShader(m_skinTexProgram, fs);
    glLinkProgram(m_skinTexProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint linked = 0;
    glGetProgramiv(m_skinTexProgram, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[1024];
        glGetProgramInfoLog(m_skinTexProgram, sizeof(log), nullptr, log);
        std::cerr << "Skinned textured program link: " << log << "\n";
        glDeleteProgram(m_skinTexProgram);
        m_skinTexProgram = 0;
    } else {
        // GLSL 330 has no `layout(binding=N)` on uniform blocks; bind block to point 1 (see UBO below).
        const GLuint skinBlock = glGetUniformBlockIndex(m_skinTexProgram, "SkinPalette");
        if (skinBlock != GL_INVALID_INDEX)
            glUniformBlockBinding(m_skinTexProgram, skinBlock, 1);
    }
}

void OpenGLRenderSystem::releaseGpuMeshesForKey(const std::string& assetCacheKey) {
    auto it = m_gpuMeshByAssetKey.find(assetCacheKey);
    if (it == m_gpuMeshByAssetKey.end())
        return;
    for (StaticMeshPart& p : it->second) {
        if (p.ebo)
            glDeleteBuffers(1, &p.ebo);
        if (p.vbo)
            glDeleteBuffers(1, &p.vbo);
        if (p.vao)
            glDeleteVertexArrays(1, &p.vao);
        if (p.albedo)
            glDeleteTextures(1, &p.albedo);
        if (p.normalMap)
            glDeleteTextures(1, &p.normalMap);
        if (p.occlusionMap)
            glDeleteTextures(1, &p.occlusionMap);
        if (p.metallicRoughnessMap)
            glDeleteTextures(1, &p.metallicRoughnessMap);
        p = {};
    }
    m_gpuMeshByAssetKey.erase(it);
}

void OpenGLRenderSystem::releaseStaticModel() {
    std::vector<std::string> keys;
    keys.reserve(m_gpuMeshByAssetKey.size());
    for (const auto& kv : m_gpuMeshByAssetKey)
        keys.push_back(kv.first);
    for (const std::string& k : keys)
        releaseGpuMeshesForKey(k);
}

bool OpenGLRenderSystem::uploadStaticModel(const ModelAsset& model, const std::string& assetCacheKey) {
    if (assetCacheKey.empty() || model.meshes.empty() || !m_window)
        return false;

    releaseGpuMeshesForKey(assetCacheKey);

    std::vector<StaticMeshPart> parts;
    parts.reserve(model.meshes.size());

    const GLsizei stride = static_cast<GLsizei>(sizeof(Vertex));

    for (const Mesh& mesh : model.meshes) {
        if (mesh.vertices.empty() || mesh.indices.empty())
            continue;

        StaticMeshPart part{};

        const Material* mat = nullptr;
        if (mesh.materialIndex >= 0 && static_cast<size_t>(mesh.materialIndex) < model.materials.size())
            mat = &model.materials[static_cast<size_t>(mesh.materialIndex)];

        part.albedo = (mat && !mat->albedoTexture.empty()) ? loadTexture2DFromFile(mat->albedoTexture, true) : 0;
        if (!part.albedo)
            part.albedo = make1x1WhiteTexture();

        if (mat && !mat->normalTexture.empty()) {
            part.normalMap = loadTexture2DFromFile(mat->normalTexture, false);
            part.hasNormalMap = part.normalMap != 0;
        }

        if (mat && !mat->occlusionTexture.empty()) {
            part.occlusionMap = loadTexture2DFromFile(mat->occlusionTexture, false);
            part.hasOcclusion = part.occlusionMap != 0;
        }

        if (mat && !mat->metallicRoughnessTexture.empty()) {
            part.metallicRoughnessMap = loadTexture2DFromFile(mat->metallicRoughnessTexture, false);
            part.hasMetallicRoughness = part.metallicRoughnessMap != 0;
        }

        glGenVertexArrays(1, &part.vao);
        glGenBuffers(1, &part.vbo);
        glGenBuffers(1, &part.ebo);
        glBindVertexArray(part.vao);
        glBindBuffer(GL_ARRAY_BUFFER, part.vbo);
        glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(Vertex), mesh.vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, part.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(int), mesh.indices.data(), GL_STATIC_DRAW);

        bool anySkin = false;
        for (const Vertex& vv : mesh.vertices) {
            const float ws =
                vv.boneWeight[0] + vv.boneWeight[1] + vv.boneWeight[2] + vv.boneWeight[3];
            if (ws > 1e-6f) {
                anySkin = true;
                break;
            }
        }
        part.skinned = anySkin;

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 3));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 6));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 8));
        glEnableVertexAttribArray(3);
        glVertexAttribIPointer(4, 4, GL_INT, stride, (void*)offsetof(Vertex, boneIndex));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, boneWeight));
        glEnableVertexAttribArray(5);

        glBindVertexArray(0);
        part.indexCount = static_cast<int>(mesh.indices.size());
        parts.push_back(std::move(part));
    }

    if (parts.empty())
        return false;

    m_gpuMeshByAssetKey[assetCacheKey] = std::move(parts);
    return true;
}

void OpenGLRenderSystem::applyTexturedSceneLighting(unsigned int program, Registry& registry, const Vec3& cameraWorld) {
    Vec3 ambient(0.06f, 0.07f, 0.09f);
    float posType[8 * 4]{};
    float dirRange[8 * 4]{};
    float colorInt[8 * 4]{};
    float spot[8 * 4]{};
    float attenSpec[8 * 4]{};
    int count = 0;
    float rimPow = 4.0f;
    float rimInt = 0.0f;

    auto entities = registry.getEntitiesWith<LightingComponent>();
    for (Entity e : entities) {
        auto& L = registry.getComponent<LightingComponent>(e);
        if (!L.enabled)
            continue;
        if (L.type == LightType::Ambient) {
            ambient.x += L.color.x * L.intensity;
            ambient.y += L.color.y * L.intensity;
            ambient.z += L.color.z * L.intensity;
            continue;
        }
        if (count >= 8)
            continue;
        if (!registry.hasComponent<WorldTransformComponent>(e))
            continue;

        const Mat4& w = registry.getComponent<WorldTransformComponent>(e).world;
        Vec3 pos{w.m[12], w.m[13], w.m[14]};
        Vec3 forward;
        if (lengthSquared(L.worldDirectionOverride) > 1e-8f)
            forward = normalize(L.worldDirectionOverride);
        else if (L.useEntityAxis) {
            forward = normalize(Vec3{w.m[8], w.m[9], w.m[10]});
        } else
            forward = Vec3{0.0f, -1.0f, 0.0f};

        const float t = static_cast<float>(L.type);
        posType[count * 4 + 0] = pos.x;
        posType[count * 4 + 1] = pos.y;
        posType[count * 4 + 2] = pos.z;
        posType[count * 4 + 3] = t;

        dirRange[count * 4 + 0] = forward.x;
        dirRange[count * 4 + 1] = forward.y;
        dirRange[count * 4 + 2] = forward.z;
        dirRange[count * 4 + 3] = L.range;

        colorInt[count * 4 + 0] = L.color.x;
        colorInt[count * 4 + 1] = L.color.y;
        colorInt[count * 4 + 2] = L.color.z;
        colorInt[count * 4 + 3] = L.intensity;

        constexpr float kPi = 3.14159265f;
        const float innerCos = std::cos(L.spotInnerDegrees * kPi / 180.0f);
        const float outerCos = std::cos(L.spotOuterDegrees * kPi / 180.0f);
        spot[count * 4 + 0] = innerCos;
        spot[count * 4 + 1] = outerCos;
        spot[count * 4 + 2] = L.attenLinear;
        spot[count * 4 + 3] = L.attenQuadratic;

        attenSpec[count * 4 + 0] = L.attenConstant;
        attenSpec[count * 4 + 1] = L.specularPower;
        attenSpec[count * 4 + 2] = L.specularIntensity;
        attenSpec[count * 4 + 3] = L.useHalfLambert ? 1.0f : 0.0f;

        if (L.rimIntensity > rimInt) {
            rimInt = L.rimIntensity;
            rimPow = L.rimPower;
        }
        ++count;
    }

    GLint uA = glGetUniformLocation(program, "uAmbient");
    GLint uCam = glGetUniformLocation(program, "uCameraPos");
    GLint uLc = glGetUniformLocation(program, "uLightCount");
    GLint uRim = glGetUniformLocation(program, "uRim");
    if (uA >= 0)
        glUniform3f(uA, ambient.x, ambient.y, ambient.z);
    if (uCam >= 0)
        glUniform3f(uCam, cameraWorld.x, cameraWorld.y, cameraWorld.z);
    if (uLc >= 0)
        glUniform1i(uLc, count);
    if (uRim >= 0)
        glUniform2f(uRim, rimPow, rimInt);

    GLint uP = glGetUniformLocation(program, "uLightPosType[0]");
    GLint uD = glGetUniformLocation(program, "uLightDirRange[0]");
    GLint uC = glGetUniformLocation(program, "uLightColorInt[0]");
    GLint uS = glGetUniformLocation(program, "uLightSpot[0]");
    GLint uAt = glGetUniformLocation(program, "uLightAttenSpec[0]");
    if (uP >= 0)
        glUniform4fv(uP, 8, posType);
    if (uD >= 0)
        glUniform4fv(uD, 8, dirRange);
    if (uC >= 0)
        glUniform4fv(uC, 8, colorInt);
    if (uS >= 0)
        glUniform4fv(uS, 8, spot);
    if (uAt >= 0)
        glUniform4fv(uAt, 8, attenSpec);
}

void OpenGLRenderSystem::applyHdriUniforms(unsigned int program, Registry& registry) {
    GLint uUse = glGetUniformLocation(program, "uUseHdri");
    if (uUse < 0)
        return;

    std::vector<Entity> entities = registry.getEntitiesWith<HdriEnvironmentComponent>();
    Entity first = INVALID_ENTITY;
    int enabledCount = 0;
    for (Entity e : entities) {
        auto& h = registry.getComponent<HdriEnvironmentComponent>(e);
        if (!h.enabled || h.hdriAssetPath.empty())
            continue;
        ++enabledCount;
        if (first == INVALID_ENTITY)
            first = e;
    }
    if (enabledCount > 1 && !m_hdriWarnedMultiple) {
        std::cerr << "OpenGLRenderSystem: multiple enabled HdriEnvironmentComponent entities; using the first only.\n";
        m_hdriWarnedMultiple = true;
    }

    if (first == INVALID_ENTITY) {
        glUniform1i(uUse, 0);
        return;
    }

    HdriEnvironmentComponent& h = registry.getComponent<HdriEnvironmentComponent>(first);
    std::string resolved;
    std::string err;
    if (resolveHdriFilesystemPath(h.hdriAssetPath) != m_hdriLoadedPath || m_hdriTexture == 0) {
        if (m_hdriTexture != 0) {
            glDeleteTextures(1, &m_hdriTexture);
            m_hdriTexture = 0;
        }
        m_hdriLoadedPath.clear();
        GLuint t = loadEquirectTextureFromFile(h.hdriAssetPath, resolved, err);
        if (t == 0) {
            std::cerr << err << "\n";
            glUniform1i(uUse, 0);
            return;
        }
        m_hdriTexture = t;
        m_hdriLoadedPath = std::move(resolved);
    }

    glUniform1i(uUse, 1);
    GLint uEnv = glGetUniformLocation(program, "uEnvMap");
    if (uEnv >= 0) {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, m_hdriTexture);
        glUniform1i(uEnv, 4);
    }
    GLint uInt = glGetUniformLocation(program, "uHdriIntensity");
    GLint uRot = glGetUniformLocation(program, "uHdriRotY");
    GLint uDf = glGetUniformLocation(program, "uHdriDiffuseW");
    GLint uSp = glGetUniformLocation(program, "uHdriSpecW");
    if (uInt >= 0)
        glUniform1f(uInt, h.intensity);
    if (uRot >= 0)
        glUniform1f(uRot, h.rotationY);
    if (uDf >= 0)
        glUniform1f(uDf, h.diffuseEnvironmentWeight);
    if (uSp >= 0)
        glUniform1f(uSp, h.specularEnvironmentWeight);
}

namespace {

/// World-space grid snap for floating origin (larger = fewer origin jumps, more precision risk at cell edges).
constexpr float kRenderOriginSnapGrid = 256.0f;

Vec3 snapRenderOrigin(const Vec3& eye)
{
    const float g = kRenderOriginSnapGrid;
    return {std::floor(eye.x / g) * g, std::floor(eye.y / g) * g, std::floor(eye.z / g) * g};
}

} // namespace

void OpenGLRenderSystem::getFloatingOriginMatrices(
    const Mat4& proj,
    const Mat4& view,
    const Vec3& snappedOrigin,
    Mat4& outPvShifted,
    Mat4& outTmO)
{
    const auto originEq = [](const Vec3& a, const Vec3& b) {
        return std::abs(a.x - b.x) < 1e-4f && std::abs(a.y - b.y) < 1e-4f && std::abs(a.z - b.z) < 1e-4f;
    };

    const bool match = m_floatOriginCacheValid && m_floatOriginCacheFbW == m_fbW && m_floatOriginCacheFbH == m_fbH &&
        std::memcmp(m_floatOriginCacheView, view.m, sizeof(view.m)) == 0 &&
        std::memcmp(m_floatOriginCacheProj, proj.m, sizeof(proj.m)) == 0 && originEq(m_floatOriginCachedO, snappedOrigin);

    if (match) {
        outPvShifted = m_floatOriginPvShifted;
        outTmO = m_floatOriginTmO;
        return;
    }

    const Mat4 T_O = Mat4::FromTranslation(snappedOrigin);
    outTmO = Mat4::FromTranslation({-snappedOrigin.x, -snappedOrigin.y, -snappedOrigin.z});
    outPvShifted = mat4Mul(proj, mat4Mul(view, T_O));

    m_floatOriginPvShifted = outPvShifted;
    m_floatOriginTmO = outTmO;
    m_floatOriginCachedO = snappedOrigin;
    std::memcpy(m_floatOriginCacheView, view.m, sizeof(view.m));
    std::memcpy(m_floatOriginCacheProj, proj.m, sizeof(proj.m));
    m_floatOriginCacheFbW = m_fbW;
    m_floatOriginCacheFbH = m_fbH;
    m_floatOriginCacheValid = true;
}

void OpenGLRenderSystem::drawTexturedModel(
    Registry& registry,
    const Mat4& pvShifted,
    const Mat4& tmO,
    const Mat4& model,
    const std::string& assetCacheKey,
    const Vec3& cameraWorld) {
    auto it = m_gpuMeshByAssetKey.find(assetCacheKey);
    if (it == m_gpuMeshByAssetKey.end() || it->second.empty() || m_texProgram == 0)
        return;

    glUseProgram(m_texProgram);

    Mat4 mvp = mat4Mul(pvShifted, mat4Mul(tmO, model));

    GLint uM = glGetUniformLocation(m_texProgram, "uModel");
    GLint uMvp = glGetUniformLocation(m_texProgram, "uMVP");
    GLint uTint = glGetUniformLocation(m_texProgram, "uTint");
    GLint uAlb = glGetUniformLocation(m_texProgram, "uAlbedo");
    GLint uNorm = glGetUniformLocation(m_texProgram, "uNormalMap");
    GLint uOcc = glGetUniformLocation(m_texProgram, "uOcclusion");
    GLint uMr = glGetUniformLocation(m_texProgram, "uMetallicRoughness");
    GLint uUseN = glGetUniformLocation(m_texProgram, "uUseNormalMap");
    GLint uUseOcc = glGetUniformLocation(m_texProgram, "uUseOcclusion");
    GLint uUseMr = glGetUniformLocation(m_texProgram, "uUseMetallicRoughness");

    glUniformMatrix4fv(uM, 1, GL_FALSE, model.m);
    glUniformMatrix4fv(uMvp, 1, GL_FALSE, mvp.m);

    glUniform3f(uTint, 1.0f, 1.0f, 1.0f);
    applyTexturedSceneLighting(m_texProgram, registry, cameraWorld);
    applyHdriUniforms(m_texProgram, registry);

    const GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (const StaticMeshPart& part : it->second) {
        glUniform1i(uUseN, part.hasNormalMap ? 1 : 0);
        glUniform1i(uUseOcc, part.hasOcclusion ? 1 : 0);
        glUniform1i(uUseMr, part.hasMetallicRoughness ? 1 : 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, part.albedo);
        glUniform1i(uAlb, 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, part.hasNormalMap ? part.normalMap : part.albedo);
        glUniform1i(uNorm, 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, part.hasOcclusion ? part.occlusionMap : part.albedo);
        glUniform1i(uOcc, 2);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, part.hasMetallicRoughness ? part.metallicRoughnessMap : part.albedo);
        glUniform1i(uMr, 3);

        glBindVertexArray(part.vao);
        glDrawElements(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);

    if (cullWas)
        glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    glUseProgram(m_program);
}

void OpenGLRenderSystem::drawTexturedSkinnedModel(
    Registry& registry,
    const Mat4& pvShifted,
    const Mat4& tmO,
    const Mat4& model,
    const std::string& assetCacheKey,
    const std::vector<Mat4>& jointSkinMatrices,
    const Vec3& cameraWorld) {
    auto it = m_gpuMeshByAssetKey.find(assetCacheKey);
    if (it == m_gpuMeshByAssetKey.end() || it->second.empty() || m_skinTexProgram == 0)
        return;

    static std::vector<Mat4> pad;
    pad.assign(static_cast<size_t>(kMaxSkinJoints), Mat4::Identity());
    const size_t n = std::min(jointSkinMatrices.size(), static_cast<size_t>(kMaxSkinJoints));
    for (size_t i = 0; i < n; ++i)
        pad[i] = jointSkinMatrices[i];

    glBindBuffer(GL_UNIFORM_BUFFER, m_skinPaletteUbo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, kSkinUboBytes, pad.data());
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_skinPaletteUbo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glUseProgram(m_skinTexProgram);

    Mat4 mvp = mat4Mul(pvShifted, mat4Mul(tmO, model));

    GLint uM = glGetUniformLocation(m_skinTexProgram, "uModel");
    GLint uMvp = glGetUniformLocation(m_skinTexProgram, "uMVP");
    GLint uTint = glGetUniformLocation(m_skinTexProgram, "uTint");
    GLint uAlb = glGetUniformLocation(m_skinTexProgram, "uAlbedo");
    GLint uNorm = glGetUniformLocation(m_skinTexProgram, "uNormalMap");
    GLint uOcc = glGetUniformLocation(m_skinTexProgram, "uOcclusion");
    GLint uMr = glGetUniformLocation(m_skinTexProgram, "uMetallicRoughness");
    GLint uUseN = glGetUniformLocation(m_skinTexProgram, "uUseNormalMap");
    GLint uUseOcc = glGetUniformLocation(m_skinTexProgram, "uUseOcclusion");
    GLint uUseMr = glGetUniformLocation(m_skinTexProgram, "uUseMetallicRoughness");

    glUniformMatrix4fv(uM, 1, GL_FALSE, model.m);
    glUniformMatrix4fv(uMvp, 1, GL_FALSE, mvp.m);

    glUniform3f(uTint, 1.0f, 1.0f, 1.0f);
    applyTexturedSceneLighting(m_skinTexProgram, registry, cameraWorld);
    applyHdriUniforms(m_skinTexProgram, registry);

    const GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (const StaticMeshPart& part : it->second) {
        if (!part.skinned)
            continue;

        glUniform1i(uUseN, part.hasNormalMap ? 1 : 0);
        glUniform1i(uUseOcc, part.hasOcclusion ? 1 : 0);
        glUniform1i(uUseMr, part.hasMetallicRoughness ? 1 : 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, part.albedo);
        glUniform1i(uAlb, 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, part.hasNormalMap ? part.normalMap : part.albedo);
        glUniform1i(uNorm, 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, part.hasOcclusion ? part.occlusionMap : part.albedo);
        glUniform1i(uOcc, 2);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, part.hasMetallicRoughness ? part.metallicRoughnessMap : part.albedo);
        glUniform1i(uMr, 3);

        glBindVertexArray(part.vao);
        glDrawElements(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT, nullptr);
    }

    bool anyRigid = false;
    for (const StaticMeshPart& part : it->second) {
        if (!part.skinned) {
            anyRigid = true;
            break;
        }
    }
    if (anyRigid) {
        glUseProgram(m_texProgram);
        GLint uMr = glGetUniformLocation(m_texProgram, "uMetallicRoughness");
        GLint uM = glGetUniformLocation(m_texProgram, "uModel");
        GLint uMvp = glGetUniformLocation(m_texProgram, "uMVP");
        GLint uTint = glGetUniformLocation(m_texProgram, "uTint");
        GLint uAlb = glGetUniformLocation(m_texProgram, "uAlbedo");
        GLint uNorm = glGetUniformLocation(m_texProgram, "uNormalMap");
        GLint uOcc = glGetUniformLocation(m_texProgram, "uOcclusion");
        GLint uUseN = glGetUniformLocation(m_texProgram, "uUseNormalMap");
        GLint uUseOcc = glGetUniformLocation(m_texProgram, "uUseOcclusion");
        GLint uUseMr = glGetUniformLocation(m_texProgram, "uUseMetallicRoughness");
        glUniformMatrix4fv(uM, 1, GL_FALSE, model.m);
        glUniformMatrix4fv(uMvp, 1, GL_FALSE, mvp.m);
        glUniform3f(uTint, 1.0f, 1.0f, 1.0f);
        applyTexturedSceneLighting(m_texProgram, registry, cameraWorld);
        applyHdriUniforms(m_texProgram, registry);
        for (const StaticMeshPart& part : it->second) {
            if (part.skinned)
                continue;
            glUniform1i(uUseN, part.hasNormalMap ? 1 : 0);
            glUniform1i(uUseOcc, part.hasOcclusion ? 1 : 0);
            glUniform1i(uUseMr, part.hasMetallicRoughness ? 1 : 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, part.albedo);
            glUniform1i(uAlb, 0);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, part.hasNormalMap ? part.normalMap : part.albedo);
            glUniform1i(uNorm, 1);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, part.hasOcclusion ? part.occlusionMap : part.albedo);
            glUniform1i(uOcc, 2);
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, part.hasMetallicRoughness ? part.metallicRoughnessMap : part.albedo);
            glUniform1i(uMr, 3);
            glBindVertexArray(part.vao);
            glDrawElements(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT, nullptr);
        }
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
    }

    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);

    if (cullWas)
        glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    glUseProgram(m_program);
}

void OpenGLRenderSystem::shutdown() {
    releaseStaticModel();
    if (m_hdriTexture != 0) {
        glDeleteTextures(1, &m_hdriTexture);
        m_hdriTexture = 0;
    }
    m_hdriLoadedPath.clear();
    m_floatOriginCacheValid = false;
    if (m_skinPaletteUbo) {
        glDeleteBuffers(1, &m_skinPaletteUbo);
        m_skinPaletteUbo = 0;
    }
    if (m_skinTexProgram) {
        glDeleteProgram(m_skinTexProgram);
        m_skinTexProgram = 0;
    }
    if (m_texProgram) {
        glDeleteProgram(m_texProgram);
        m_texProgram = 0;
    }
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
        glfwTerminate();
    }
}

void OpenGLRenderSystem::pollFramebufferSize(int& outW, int& outH) const {
    if (m_window)
        glfwGetFramebufferSize(m_window, &outW, &outH);
    else {
        outW = m_fbW;
        outH = m_fbH;
    }
}

bool OpenGLRenderSystem::shouldClose() const {
    return m_window && glfwWindowShouldClose(m_window);
}

void OpenGLRenderSystem::drawPyramid(const Mat4& mvp, const Mat4& model, const float color[3]) {
    GLint uM = glGetUniformLocation(m_program, "uModel");
    GLint uMvp = glGetUniformLocation(m_program, "uMVP");
    GLint uC = glGetUniformLocation(m_program, "uColor");
    GLint uL = glGetUniformLocation(m_program, "uLightDir");
    GLint uA = glGetUniformLocation(m_program, "uAmbient");
    glUniformMatrix4fv(uM, 1, GL_FALSE, model.m);
    glUniformMatrix4fv(uMvp, 1, GL_FALSE, mvp.m);
    glUniform3fv(uC, 1, color);
    float lightDir[3] = {0.35f, 0.85f, 0.35f};
    float len = std::sqrt(lightDir[0] * lightDir[0] + lightDir[1] * lightDir[1] + lightDir[2] * lightDir[2]);
    if (len > 1e-5f) {
        lightDir[0] /= len;
        lightDir[1] /= len;
        lightDir[2] /= len;
    }
    glUniform3fv(uL, 1, lightDir);
    float amb[3] = {0.22f, 0.22f, 0.25f};
    glUniform3fv(uA, 1, amb);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertexCount));
    glBindVertexArray(0);
}

void OpenGLRenderSystem::renderFrame(Registry& registry) {
    if (!m_window || !m_program)
        return;

    glfwGetFramebufferSize(m_window, &m_fbW, &m_fbH);
    if (m_fbW <= 0 || m_fbH <= 0)
        return;

    glViewport(0, 0, m_fbW, m_fbH);
    glClearColor(0.12f, 0.12f, 0.16f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Entity cameraEntity = INVALID_ENTITY;
    {
        auto cams = registry.getEntitiesWith<CameraComponent, TransformComponent>();
        for (Entity e : cams)
        {
            if (registry.getComponent<CameraComponent>(e).active)
            {
                cameraEntity = e;
                break;
            }
        }
    }
    if (cameraEntity == INVALID_ENTITY)
        return;

    auto& cam = registry.getComponent<CameraComponent>(cameraEntity);

    auto worldTranslation = [&](Entity e) -> Vec3 {
        if (registry.hasComponent<WorldTransformComponent>(e)) {
            const Mat4& w = registry.getComponent<WorldTransformComponent>(e).world;
            return {w.m[12], w.m[13], w.m[14]};
        }
        if (registry.hasComponent<TransformComponent>(e))
            return registry.getComponent<TransformComponent>(e).position;
        return {0.0f, 0.0f, 0.0f};
    };

    const Vec3 eye = worldTranslation(cameraEntity);

    Entity lookTarget = INVALID_ENTITY;
    if (cam.enableLockOn && cam.lockOnTarget != INVALID_ENTITY)
        lookTarget = cam.lockOnTarget;
    else if (cam.enableLookAt && cam.lookAtTarget != INVALID_ENTITY)
        lookTarget = cam.lookAtTarget;

    Mat4 view;
    if (lookTarget != INVALID_ENTITY && registry.hasComponent<TransformComponent>(lookTarget)) {
        const auto& t = registry.getComponent<TransformComponent>(lookTarget);
        Vec3 center{
            t.position.x + cam.lookAtOffset.x,
            t.position.y + cam.lookAtOffset.y,
            t.position.z + cam.lookAtOffset.z};
        view = Mat4::LookAt(eye, center, {0.0f, 1.0f, 0.0f});
    } else {
        // Fallback: match `CameraSystem` (stores camera *world* matrix in `viewMatrix`).
        view = Mat4::inverse(cam.viewMatrix);
    }

    float aspect = static_cast<float>(m_fbW) / static_cast<float>(m_fbH);
    Mat4 proj = Mat4::Perspective(cam.fov, aspect, cam.nearPlane, cam.farPlane);
    const Vec3 snappedOrigin = snapRenderOrigin(eye);
    Mat4 pvShifted;
    Mat4 tmO;
    getFloatingOriginMatrices(proj, view, snappedOrigin, pvShifted, tmO);

    Entity playerEntity = INVALID_ENTITY;
    for (Entity e : registry.getEntitiesWith<PlayerTagComponent, TransformComponent>()) {
        playerEntity = e;
        break;
    }

    for (Entity e : registry.getEntitiesWith<StaticMeshComponent, TransformComponent>())
    {
        auto& smc = registry.getComponent<StaticMeshComponent>(e);
        if (!smc.gpuRegistered || smc.assetCacheKey.empty())
            continue;
        if (m_gpuMeshByAssetKey.find(smc.assetCacheKey) == m_gpuMeshByAssetKey.end())
            continue;

        Mat4 base = Mat4::Identity();
        if (registry.hasComponent<WorldTransformComponent>(e))
            base = registry.getComponent<WorldTransformComponent>(e).world;
        else {
            const auto& t = registry.getComponent<TransformComponent>(e);
            base = Mat4::FromTRS(t.position, t.rotation, t.scale);
        }

        const Mat4 R = Mat4::FromTR({0.0f, 0.0f, 0.0f}, smc.modelSpaceRotation);
        const Mat4 S = Mat4::FromScale({smc.uniformScale, smc.uniformScale, smc.uniformScale});
        const Mat4 combined = mat4Mul(mat4Mul(base, R), S);

        if (registry.hasComponent<GpuSkinPaletteComponent>(e)) {
            const auto& skinPal = registry.getComponent<GpuSkinPaletteComponent>(e);
            if (!skinPal.jointSkinMatrices.empty())
                drawTexturedSkinnedModel(registry, pvShifted, tmO, combined, smc.assetCacheKey, skinPal.jointSkinMatrices, eye);
            else
                drawTexturedModel(registry, pvShifted, tmO, combined, smc.assetCacheKey, eye);
        } else {
            drawTexturedModel(registry, pvShifted, tmO, combined, smc.assetCacheKey, eye);
        }
    }

    glUseProgram(m_program);

    auto drawAt = [&](const Vec3& pos, float scale, const float col[3]) {
        Mat4 model = mat4Mul(Mat4::FromTranslation(pos), Mat4::FromScale({scale, scale, scale}));
        Mat4 mvp = mat4Mul(pvShifted, mat4Mul(tmO, model));
        drawPyramid(mvp, model, col);
    };

    const float colPlayer[3] = {0.2f, 0.75f, 0.35f};
    const float colEnemy[3] = {0.85f, 0.35f, 0.2f};
    const float colBone[3] = {0.9f, 0.85f, 0.2f};

    bool playerHasRenderableMesh = false;
    if (playerEntity != INVALID_ENTITY && registry.hasComponent<StaticMeshComponent>(playerEntity)) {
        const auto& sm = registry.getComponent<StaticMeshComponent>(playerEntity);
        playerHasRenderableMesh =
            sm.gpuRegistered && !sm.assetCacheKey.empty() &&
            m_gpuMeshByAssetKey.find(sm.assetCacheKey) != m_gpuMeshByAssetKey.end();
    }

    if (playerEntity != INVALID_ENTITY && !playerHasRenderableMesh) {
        drawAt(worldTranslation(playerEntity), 1.15f, colPlayer);
    }

    for (Entity e : registry.getEntitiesWith<EnemyTagComponent, TransformComponent>()) {
        const auto& t = registry.getComponent<TransformComponent>(e);
        const float sx = std::max(std::max(t.scale.x, t.scale.y), t.scale.z);
        drawAt(worldTranslation(e), 1.05f * sx, colEnemy);
    }

    for (Entity e : registry.getEntitiesWith<SocketComponent>()) {
        const auto& sock = registry.getComponent<SocketComponent>(e);
        if (!sock.debugDrawPyramid)
            continue;
        const Mat4& wt = sock.worldTransform;
        const Vec3 p{wt.m[12], wt.m[13], wt.m[14]};
        drawAt(p, 0.28f, colBone);
    }

    for (Entity e : registry.getEntitiesWith<BoneInstanceComponent, WorldTransformComponent>())
    {
        const Mat4& w = registry.getComponent<WorldTransformComponent>(e).world;
        const float s = 0.45f;
        Mat4 model = mat4Mul(w, Mat4::FromScale({s, s, s}));
        Mat4 mvp = mat4Mul(pvShifted, mat4Mul(tmO, model));
        drawPyramid(mvp, model, colBone);
    }

    glfwSwapBuffers(m_window);
}
