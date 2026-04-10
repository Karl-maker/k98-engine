#include "OpenGLVer2Renderer.hpp"
#include "../IGraphicsRenderer.hpp"
#include "../RenderPipeline.hpp"

#include "../../core/assets/AssetManager.hpp"
#include "../../core/assets/IAsset.hpp"
#include "../../core/assets/ModelAsset.hpp"
#include "../../components/PbrMaterialPresetComponent.hpp"
#include "../../ecs/Entity.hpp"
#include "../../ecs/Registry.hpp"
#include "../../math/Mat4.hpp"
#include "../../math/MeshVertexStream.hpp"
#include "../../components/CameraComponent.hpp"
#include "../../components/GpuSkinPaletteComponent.hpp"
#include "../../components/RenderableMeshComponent.hpp"
#include "../../components/StaticMeshMaterialOverrideComponent.hpp"
#include "../../components/PrimitiveBoxComponent.hpp"
#include "../../components/RaycastComponent.hpp"
#include "../../components/PrimitivePyramidComponent.hpp"
#include "../../components/PlayerTagComponent.hpp"
#include "../../components/TransformComponent.hpp"
#include "../../components/HdriEnvironmentComponent.hpp"
#include "../../components/LightingComponent.hpp"
#include "../../components/WorldTransformComponent.hpp"
#include "../../components/TerrainChunkComponent.hpp"
#include "../../components/HeightMapComponent.hpp"
#include "../../math/MathOps.hpp"
#include "../../math/Vec3.hpp"

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
#include <memory>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "stb_image.h"
#include "stb_easy_font.h"
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

// Pixel coordinates: origin top-left, +Y downward (matches stb_easy_font). Maps to clip space in-shader.
static const char* kHudVertSrc = R"(
#version 330 core
layout (location = 0) in vec2 aPixelPos;
layout (location = 1) in vec4 aColor;
uniform vec2 uFbSize;
out vec4 vColor;
void main() {
    vColor = aColor;
    vec2 ndc;
    ndc.x = (aPixelPos.x / uFbSize.x) * 2.0 - 1.0;
    ndc.y = 1.0 - (aPixelPos.y / uFbSize.y) * 2.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)";

static const char* kHudFragSrc = R"(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
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
/// Bound to texture unit 5 so it does not clash with albedo–MR (0–3) or HDRI (4).
uniform sampler2D uDisplacement;
uniform int uUseDisplacement;
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

    float dispCavity = 1.0;
    if (uUseDisplacement != 0)
        dispCavity = mix(0.88, 1.0, texture(uDisplacement, vUV).r);

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

    vec3 ambLit = uAmbient * occ * dispCavity;
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
    GLint internal = GL_RGBA8;
    if (srgb) {
#if defined(GL_SRGB8_ALPHA8)
        internal = GL_SRGB8_ALPHA8;
#endif
    }
    glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    stbi_image_free(data);
    return tex;
}

/// Ensures fragment shader displacement uniforms are bound (unit 5); static meshes do not sample height yet.
static void bindDisplacementSlotUnused(GLuint program, GLuint whiteTex)
{
    if (!whiteTex)
        return;
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, whiteTex);
    const GLint uDisp = glGetUniformLocation(program, "uDisplacement");
    const GLint uUseDisp = glGetUniformLocation(program, "uUseDisplacement");
    if (uDisp >= 0)
        glUniform1i(uDisp, 5);
    if (uUseDisp >= 0)
        glUniform1i(uUseDisp, 0);
    glActiveTexture(GL_TEXTURE0);
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

bool OpenGLVer2Renderer::init(int width, int height, const char* title, const OpenGLInitOptions& options) {
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
    glfwSwapInterval(options.swapInterval);
    glfwShowWindow(m_window);

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
    buildBoxMesh();
    buildDebugLineMesh();
    buildTexturedShaderPipeline();
    buildSkinnedTexturedShaderPipeline();

    glGenBuffers(1, &m_skinPaletteUbo);
    glBindBuffer(GL_UNIFORM_BUFFER, m_skinPaletteUbo);
    glBufferData(GL_UNIFORM_BUFFER, kSkinUboBytes, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_skinPaletteUbo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    installDefaultRenderPasses();

    buildDebugHudPipeline();

    m_texWhite1x1 = make1x1WhiteTexture();

    return true;
}

void OpenGLVer2Renderer::buildPyramidMesh() {
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

void OpenGLVer2Renderer::buildBoxMesh()
{
    struct V {
        float px, py, pz, nx, ny, nz;
    };
    std::vector<V> v;
    const float h = 0.5f;
    auto addTri = [&](const Vec3& a, const Vec3& b, const Vec3& c) {
        Vec3 e1{b.x - a.x, b.y - a.y, b.z - a.z};
        Vec3 e2{c.x - a.x, c.y - a.y, c.z - a.z};
        Vec3 n = normalize(cross(e1, e2));
        v.push_back({a.x, a.y, a.z, n.x, n.y, n.z});
        v.push_back({b.x, b.y, b.z, n.x, n.y, n.z});
        v.push_back({c.x, c.y, c.z, n.x, n.y, n.z});
    };

    // Unit cube [-0.5,0.5]^3, CCW faces out.
    addTri({h, -h, -h}, {h, h, -h}, {h, h, h});
    addTri({h, -h, -h}, {h, h, h}, {h, -h, h});
    addTri({-h, -h, h}, {-h, h, h}, {-h, h, -h});
    addTri({-h, -h, h}, {-h, h, -h}, {-h, -h, -h});
    addTri({-h, h, -h}, {h, h, -h}, {h, h, h});
    addTri({-h, h, -h}, {h, h, h}, {-h, h, h});
    addTri({-h, -h, h}, {h, -h, h}, {h, -h, -h});
    addTri({-h, -h, h}, {h, -h, -h}, {-h, -h, -h});
    addTri({-h, -h, h}, {h, -h, h}, {h, h, h});
    addTri({-h, -h, h}, {h, h, h}, {-h, h, h});
    addTri({-h, -h, -h}, {-h, h, -h}, {h, h, -h});
    addTri({-h, -h, -h}, {h, h, -h}, {h, -h, -h});

    m_boxVertexCount = static_cast<unsigned int>(v.size());
    glGenVertexArrays(1, &m_boxVao);
    glGenBuffers(1, &m_boxVbo);
    glBindVertexArray(m_boxVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_boxVbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(V), v.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void OpenGLVer2Renderer::buildDebugLineMesh()
{
    glGenVertexArrays(1, &m_lineVao);
    glGenBuffers(1, &m_lineVbo);
    glBindVertexArray(m_lineVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 12, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void OpenGLVer2Renderer::drawDebugRaycasts(RenderContext& ctx)
{
    if (!ctx.registry || !m_lineVao || !m_program)
        return;
    Registry& registry = *ctx.registry;

    glUseProgram(m_program);
    GLint uM = glGetUniformLocation(m_program, "uModel");
    GLint uMvp = glGetUniformLocation(m_program, "uMVP");
    GLint uC = glGetUniformLocation(m_program, "uColor");
    GLint uL = glGetUniformLocation(m_program, "uLightDir");
    GLint uA = glGetUniformLocation(m_program, "uAmbient");
    const float lightDir[3] = {0.35f, 0.85f, 0.35f};
    float len = std::sqrt(lightDir[0] * lightDir[0] + lightDir[1] * lightDir[1] + lightDir[2] * lightDir[2]);
    float ld[3] = {lightDir[0] / len, lightDir[1] / len, lightDir[2] / len};
    const float amb[3] = {0.35f, 0.35f, 0.38f};
    glUniform3fv(uL, 1, ld);
    glUniform3fv(uA, 1, amb);

    const Mat4 model = Mat4::Identity();
    glUniformMatrix4fv(uM, 1, GL_FALSE, model.m);

    const GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);
    glDepthFunc(GL_LEQUAL);

    for (Entity e : registry.getEntitiesWith<RaycastComponent, TransformComponent>()) {
        const auto& rc = registry.getComponent<RaycastComponent>(e);
        if (!rc.debugDraw)
            continue;

        const Vec3& a = rc.lastWorldOrigin;
        const Mat4 mvp = mat4Mul(ctx.pvShifted, mat4Mul(ctx.tmO, model));
        glUniformMatrix4fv(uMvp, 1, GL_FALSE, mvp.m);
        glBindVertexArray(m_lineVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_lineVbo);

        const int nLines = rc.debugRayCount > 0 ? rc.debugRayCount : 1;
        for (int li = 0; li < nLines; ++li) {
            Vec3 b = rc.lastRayEnd;
            if (rc.debugRayCount > 0)
                b = rc.debugRayEnd[li];
            float v[12] = {
                a.x, a.y, a.z, 0.f, 1.f, 0.f,
                b.x, b.y, b.z, 0.f, 1.f, 0.f,
            };
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
            const float dim = (rc.debugRayCount > 1 && li > 0) ? 0.45f : 1.f;
            const float colRay[3] = {0.15f * dim + 0.05f, 0.85f * dim + 0.1f, 0.25f * dim + 0.1f};
            glUniform3fv(uC, 1, colRay);
            glDrawArrays(GL_LINES, 0, 2);
        }

        if (rc.hasHit) {
            const Vec3 n = normalize(rc.hitNormal);
            const Vec3 h = rc.hitPoint;
            const float nh = 0.35f;
            float v2[12] = {
                h.x, h.y, h.z, 0.f, 0.f, 1.f,
                h.x + n.x * nh, h.y + n.y * nh, h.z + n.z * nh, 0.f, 0.f, 1.f,
            };
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v2), v2);
            const float colN[3] = {1.f, 0.45f, 0.15f};
            glUniform3fv(uC, 1, colN);
            glDrawArrays(GL_LINES, 0, 2);
        }
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    if (cullWas)
        glEnable(GL_CULL_FACE);
}

void OpenGLVer2Renderer::buildTexturedShaderPipeline() {
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

void OpenGLVer2Renderer::buildSkinnedTexturedShaderPipeline() {
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

void OpenGLVer2Renderer::buildDebugHudPipeline()
{
    GLuint vs = compileShader(GL_VERTEX_SHADER, kHudVertSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kHudFragSrc);
    if (!vs || !fs)
        return;
    m_debugHudProgram = glCreateProgram();
    glAttachShader(m_debugHudProgram, vs);
    glAttachShader(m_debugHudProgram, fs);
    glLinkProgram(m_debugHudProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint linked = 0;
    glGetProgramiv(m_debugHudProgram, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[1024];
        glGetProgramInfoLog(m_debugHudProgram, sizeof(log), nullptr, log);
        std::cerr << "Debug HUD program link: " << log << "\n";
        glDeleteProgram(m_debugHudProgram);
        m_debugHudProgram = 0;
        return;
    }

    m_debugHudLocFbSize = glGetUniformLocation(m_debugHudProgram, "uFbSize");

    glGenVertexArrays(1, &m_debugHudVao);
    glGenBuffers(1, &m_debugHudVbo);
    glBindVertexArray(m_debugHudVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_debugHudVbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(6 * sizeof(float)), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        1,
        4,
        GL_FLOAT,
        GL_FALSE,
        static_cast<GLsizei>(6 * sizeof(float)),
        (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void OpenGLVer2Renderer::setDebugHudSnapshot(OpenGLDebugHudSnapshot snapshot)
{
    m_debugHud = std::move(snapshot);
}

void OpenGLVer2Renderer::drawDebugHudOverlay()
{
    if (!m_debugHud.enabled || !m_debugHudProgram || !m_window)
        return;

    constexpr float kHudScale = 1.52f;
    auto scaleHudVerts = [](std::vector<float>& tri, float ax, float ay, float s) {
        for (size_t i = 0; i < tri.size(); i += 6) {
            tri[i] = ax + (tri[i] - ax) * s;
            tri[i + 1] = ay + (tri[i + 1] - ay) * s;
        }
    };

    char text[768];
    if (m_debugHud.hudHealthMax > 0.f && m_debugHud.hudHealthCurrent >= 0.f) {
        std::snprintf(
            text,
            sizeof(text),
            "FPS: %.1f (preset %d)\nEntities: %d\nLocomotion: %.40s\nState: %.32s\nHealth: %.0f / %.0f",
            static_cast<double>(m_debugHud.fps),
            m_debugHud.targetFpsPreset,
            m_debugHud.entityCount,
            m_debugHud.locomotionState.empty() ? "-" : m_debugHud.locomotionState.c_str(),
            m_debugHud.movementState.empty() ? "-" : m_debugHud.movementState.c_str(),
            static_cast<double>(m_debugHud.hudHealthCurrent),
            static_cast<double>(m_debugHud.hudHealthMax));
    } else {
        std::snprintf(
            text,
            sizeof(text),
            "FPS: %.1f (preset %d)\nEntities: %d\nLocomotion: %.40s\nState: %.32s",
            static_cast<double>(m_debugHud.fps),
            m_debugHud.targetFpsPreset,
            m_debugHud.entityCount,
            m_debugHud.locomotionState.empty() ? "-" : m_debugHud.locomotionState.c_str(),
            m_debugHud.movementState.empty() ? "-" : m_debugHud.movementState.c_str());
    }

    const int textW = stb_easy_font_width(text);
    const int textH = stb_easy_font_height(text);
    const float pad = 8.0f;
    const float textX = pad + 4.0f;
    const float textY = pad + 4.0f;
    const float panelX0 = 2.0f;
    const float panelY0 = 2.0f;
    const float panelX1 = std::max(400.0f, textX + static_cast<float>(textW) + pad + 4.0f);
    const float panelY1 = textY + static_cast<float>(textH) + pad + 4.0f;

    alignas(16) unsigned char stbBuf[256000];
    const int numQuads =
        stb_easy_font_print(textX, textY, text, nullptr, stbBuf, static_cast<int>(sizeof(stbBuf)));

    std::vector<float> tri;
    tri.reserve(static_cast<size_t>(numQuads) * 6u * 6u + 48u);

    auto pushVert = [&](float x, float y, float r, float g, float b, float a) {
        tri.push_back(x);
        tri.push_back(y);
        tri.push_back(r);
        tri.push_back(g);
        tri.push_back(b);
        tri.push_back(a);
    };

    auto pushVertBytes = [&](float x, float y, const unsigned char* rgba) {
        pushVert(
            x,
            y,
            static_cast<float>(rgba[0]) / 255.0f,
            static_cast<float>(rgba[1]) / 255.0f,
            static_cast<float>(rgba[2]) / 255.0f,
            static_cast<float>(rgba[3]) / 255.0f);
    };

    // Opaque-ish panel behind text (top-left, pixel space, +Y down) so the HUD is always visible.
    const float pr = 0.06f, pg = 0.06f, pb = 0.09f, pa = 0.94f;
    pushVert(panelX0, panelY0, pr, pg, pb, pa);
    pushVert(panelX1, panelY0, pr, pg, pb, pa);
    pushVert(panelX1, panelY1, pr, pg, pb, pa);
    pushVert(panelX0, panelY0, pr, pg, pb, pa);
    pushVert(panelX1, panelY1, pr, pg, pb, pa);
    pushVert(panelX0, panelY1, pr, pg, pb, pa);

    for (int q = 0; q < numQuads; ++q) {
        const unsigned char* base = stbBuf + static_cast<size_t>(q) * 64u;
        float x0, y0, x1, y1, x2, y2, x3, y3;
        unsigned char c0[4], c1[4], c2[4], c3[4];
        std::memcpy(&x0, base + 0, 4);
        std::memcpy(&y0, base + 4, 4);
        std::memcpy(c0, base + 12, 4);
        std::memcpy(&x1, base + 16, 4);
        std::memcpy(&y1, base + 20, 4);
        std::memcpy(c1, base + 28, 4);
        std::memcpy(&x2, base + 32, 4);
        std::memcpy(&y2, base + 36, 4);
        std::memcpy(c2, base + 44, 4);
        std::memcpy(&x3, base + 48, 4);
        std::memcpy(&y3, base + 52, 4);
        std::memcpy(c3, base + 60, 4);

        pushVertBytes(x0, y0, c0);
        pushVertBytes(x1, y1, c1);
        pushVertBytes(x2, y2, c2);
        pushVertBytes(x0, y0, c0);
        pushVertBytes(x2, y2, c2);
        pushVertBytes(x3, y3, c3);
    }

    scaleHudVerts(tri, panelX0, panelY0, kHudScale);

    if (!m_debugHud.debugDetail.empty()) {
        char detailBuf[768];
        std::snprintf(detailBuf, sizeof(detailBuf), "%s", m_debugHud.debugDetail.c_str());
        char* dtext = detailBuf;
        const int dtw = stb_easy_font_width(dtext);
        const int dth = stb_easy_font_height(dtext);
        const float dtextX = static_cast<float>(m_fbW) - pad - static_cast<float>(dtw) - 4.0f;
        const float dtextY = pad + 4.0f;
        const float dpanelX0 = dtextX - 4.0f;
        const float dpanelX1 = static_cast<float>(m_fbW) - 2.0f;
        const float dpanelY0 = 2.0f;
        const float dpanelY1 = dtextY + static_cast<float>(dth) + pad + 4.0f;

        const int dnumQuads =
            stb_easy_font_print(dtextX, dtextY, dtext, nullptr, stbBuf, static_cast<int>(sizeof(stbBuf)));

        std::vector<float> triDetail;
        triDetail.reserve(static_cast<size_t>(dnumQuads) * 6u * 6u + 48u);
        auto pushVertD = [&](float x, float y, float r, float g, float b, float a) {
            triDetail.push_back(x);
            triDetail.push_back(y);
            triDetail.push_back(r);
            triDetail.push_back(g);
            triDetail.push_back(b);
            triDetail.push_back(a);
        };
        auto pushVertBytesD = [&](float x, float y, const unsigned char* rgba) {
            pushVertD(
                x,
                y,
                static_cast<float>(rgba[0]) / 255.0f,
                static_cast<float>(rgba[1]) / 255.0f,
                static_cast<float>(rgba[2]) / 255.0f,
                static_cast<float>(rgba[3]) / 255.0f);
        };

        pushVertD(dpanelX0, dpanelY0, pr, pg, pb, pa);
        pushVertD(dpanelX1, dpanelY0, pr, pg, pb, pa);
        pushVertD(dpanelX1, dpanelY1, pr, pg, pb, pa);
        pushVertD(dpanelX0, dpanelY0, pr, pg, pb, pa);
        pushVertD(dpanelX1, dpanelY1, pr, pg, pb, pa);
        pushVertD(dpanelX0, dpanelY1, pr, pg, pb, pa);

        for (int q = 0; q < dnumQuads; ++q) {
            const unsigned char* base = stbBuf + static_cast<size_t>(q) * 64u;
            float x0, y0, x1, y1, x2, y2, x3, y3;
            unsigned char c0[4], c1[4], c2[4], c3[4];
            std::memcpy(&x0, base + 0, 4);
            std::memcpy(&y0, base + 4, 4);
            std::memcpy(c0, base + 12, 4);
            std::memcpy(&x1, base + 16, 4);
            std::memcpy(&y1, base + 20, 4);
            std::memcpy(c1, base + 28, 4);
            std::memcpy(&x2, base + 32, 4);
            std::memcpy(&y2, base + 36, 4);
            std::memcpy(c2, base + 44, 4);
            std::memcpy(&x3, base + 48, 4);
            std::memcpy(&y3, base + 52, 4);
            std::memcpy(c3, base + 60, 4);

            pushVertBytesD(x0, y0, c0);
            pushVertBytesD(x1, y1, c1);
            pushVertBytesD(x2, y2, c2);
            pushVertBytesD(x0, y0, c0);
            pushVertBytesD(x2, y2, c2);
            pushVertBytesD(x3, y3, c3);
        }

        scaleHudVerts(triDetail, dpanelX0, dpanelY0, kHudScale);
        tri.insert(tri.end(), triDetail.begin(), triDetail.end());
    }

    const GLsizei vertCount = static_cast<GLsizei>(tri.size() / 6u);
    if (vertCount <= 0)
        return;

    // Other passes may leave culling / scissor / depth mask in a state that hides screen-space HUD
    // (especially back-face cull with stb_easy_font quad winding). Reset for the overlay pass.
    const GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendWas = glIsEnabled(GL_BLEND);
    const GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
    const GLboolean scissorWas = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean colorMask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);

    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glViewport(0, 0, m_fbW, m_fbH);

    glUseProgram(m_debugHudProgram);
    if (m_debugHudLocFbSize >= 0)
        glUniform2f(m_debugHudLocFbSize, static_cast<float>(m_fbW), static_cast<float>(m_fbH));

    glBindVertexArray(m_debugHudVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_debugHudVbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(tri.size() * sizeof(float)),
        tri.data(),
        GL_DYNAMIC_DRAW);
    // Re-specify attribs after buffer orphaning (some drivers — notably macOS — need this for dynamic VBOs).
    const GLsizei stride = static_cast<GLsizei>(6 * sizeof(float));
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLES, 0, vertCount);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);

    glDepthMask(GL_TRUE);
    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    if (depthWas)
        glEnable(GL_DEPTH_TEST);
    if (!blendWas)
        glDisable(GL_BLEND);
    if (cullWas)
        glEnable(GL_CULL_FACE);
    if (scissorWas)
        glEnable(GL_SCISSOR_TEST);
}

void OpenGLVer2Renderer::releaseGpuMeshesForKey(const std::string& assetCacheKey) {
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

void OpenGLVer2Renderer::releaseStaticModel() {
    std::vector<std::string> keys;
    keys.reserve(m_gpuMeshByAssetKey.size());
    for (const auto& kv : m_gpuMeshByAssetKey)
        keys.push_back(kv.first);
    for (const std::string& k : keys)
        releaseGpuMeshesForKey(k);
}

bool OpenGLVer2Renderer::uploadStaticModel(const ModelAsset& model, const std::string& assetCacheKey) {
    if (assetCacheKey.empty() || model.meshes.empty() || !m_window)
        return false;

    releaseGpuMeshesForKey(assetCacheKey);

    std::vector<StaticMeshPart> parts;
    parts.reserve(model.meshes.size());

    const GLsizei stride = static_cast<GLsizei>(sizeof(MeshVertexStream));

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

        std::vector<MeshVertexStream> interleaved(mesh.vertices.size());
        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            interleaved[i].vertex = mesh.vertices[i];
            interleaved[i].bone =
                (i < mesh.boneData.size()) ? mesh.boneData[i] : VertexBoneData{};
        }
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(interleaved.size() * sizeof(MeshVertexStream)),
            interleaved.data(),
            GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, part.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(int), mesh.indices.data(), GL_STATIC_DRAW);

        bool anySkin = false;
        if (!mesh.boneData.empty()) {
            for (const VertexBoneData& sk : mesh.boneData) {
                const float ws =
                    sk.weights[0] + sk.weights[1] + sk.weights[2] + sk.weights[3];
                if (ws > 1e-6f) {
                    anySkin = true;
                    break;
                }
            }
        }
        part.skinned = anySkin;

        using MS = MeshVertexStream;
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(MS, vertex.position));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(MS, vertex.normal));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(MS, vertex.uv));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(MS, vertex.tangent));
        glEnableVertexAttribArray(3);
        glVertexAttribIPointer(4, 4, GL_INT, stride, (void*)offsetof(MS, bone.boneIndices));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(MS, bone.weights));
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

bool OpenGLVer2Renderer::uploadStaticModelFromPath(
    AssetManager& assets,
    const std::string& path,
    const std::string& assetCacheKey,
    bool releaseCpuMeshAfterUpload)
{
    std::shared_ptr<IAsset> a = assets.load(path);
    auto model = std::dynamic_pointer_cast<ModelAsset>(a);
    if (!model || model->meshes.empty())
        return false;
    if (!uploadStaticModel(*model, assetCacheKey))
        return false;
    if (releaseCpuMeshAfterUpload)
        model->releaseMeshGeometry();
    return true;
}

void OpenGLVer2Renderer::applyTexturedSceneLighting(unsigned int program, Registry& registry, const Vec3& cameraWorld) {
    Vec3 ambient{0.06f, 0.07f, 0.09f};
    for (Entity he : registry.getEntitiesWith<HdriEnvironmentComponent>()) {
        const auto& h = registry.getComponent<HdriEnvironmentComponent>(he);
        if (h.enabled && !h.hdriAssetPath.empty()) {
            ambient = {0.0f, 0.0f, 0.0f};
            break;
        }
    }
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

void OpenGLVer2Renderer::uploadPbrMaterialPresets(Registry& registry)
{
    for (Entity e : registry.getEntitiesWith<PbrMaterialPresetComponent>()) {
        auto& surf = registry.getComponent<PbrMaterialPresetComponent>(e);
        if (surf.gpuUploaded || surf.presetRootRelative.empty())
            continue;

        std::string root;
#ifdef GAME_ENGINE_PROJECT_ROOT
        root = std::string(GAME_ENGINE_PROJECT_ROOT);
        if (!root.empty() && root.back() != '/')
            root += '/';
#endif
        root += surf.presetRootRelative;
        if (!root.empty() && root.back() != '/')
            root += '/';

        auto load = [&](const char* file, bool srgb) -> GLuint { return loadTexture2DFromFile(root + file, srgb); };

        surf.maps.albedo = load("Tiles071_1K-JPG_Color.jpg", true);
        surf.maps.normalMap = load("Tiles071_1K-JPG_NormalGL.jpg", false);
        surf.maps.occlusionMap = load("Tiles071_1K-JPG_AmbientOcclusion.jpg", false);
        surf.maps.roughnessMap = load("Tiles071_1K-JPG_Roughness.jpg", false);
        surf.maps.displacementMap = load("Tiles071_1K-JPG_Displacement.jpg", false);

        if (surf.maps.albedo != 0)
            surf.gpuUploaded = true;
        else
            std::cerr << "PbrMaterialPreset: could not load albedo from preset: " << root << "\n";
        break;
    }
    releaseTerrainMeshes();
}

void OpenGLVer2Renderer::bindPbrTextureMaps(unsigned int program, const PbrTextureSetComponent& maps, bool useDisplacementMap)
{
    const GLuint white = m_texWhite1x1 ? m_texWhite1x1 : 0;

    GLint uAlb = glGetUniformLocation(program, "uAlbedo");
    GLint uNorm = glGetUniformLocation(program, "uNormalMap");
    GLint uOcc = glGetUniformLocation(program, "uOcclusion");
    GLint uMr = glGetUniformLocation(program, "uMetallicRoughness");
    GLint uUseN = glGetUniformLocation(program, "uUseNormalMap");
    GLint uUseOcc = glGetUniformLocation(program, "uUseOcclusion");
    GLint uUseMr = glGetUniformLocation(program, "uUseMetallicRoughness");
    GLint uDisp = glGetUniformLocation(program, "uDisplacement");
    GLint uUseDisp = glGetUniformLocation(program, "uUseDisplacement");

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, maps.albedo ? maps.albedo : white);
    if (uAlb >= 0)
        glUniform1i(uAlb, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, maps.normalMap ? maps.normalMap : white);
    if (uNorm >= 0)
        glUniform1i(uNorm, 1);
    if (uUseN >= 0)
        glUniform1i(uUseN, maps.normalMap ? 1 : 0);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, maps.occlusionMap ? maps.occlusionMap : white);
    if (uOcc >= 0)
        glUniform1i(uOcc, 2);
    if (uUseOcc >= 0)
        glUniform1i(uUseOcc, maps.occlusionMap ? 1 : 0);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, maps.roughnessMap ? maps.roughnessMap : white);
    if (uMr >= 0)
        glUniform1i(uMr, 3);
    if (uUseMr >= 0)
        glUniform1i(uUseMr, maps.roughnessMap ? 1 : 0);

    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, maps.displacementMap ? maps.displacementMap : white);
    if (uDisp >= 0)
        glUniform1i(uDisp, 5);
    if (uUseDisp >= 0)
        glUniform1i(uUseDisp, (useDisplacementMap && maps.displacementMap) ? 1 : 0);

    glActiveTexture(GL_TEXTURE0);
}

void OpenGLVer2Renderer::applyHdriUniforms(unsigned int program, Registry& registry) {
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
        std::cerr << "OpenGLVer2Renderer: multiple enabled HdriEnvironmentComponent entities; using the first only.\n";
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

/// Floating-origin reference: camera position without coarse grid snapping (256³ grid caused pops/jitter).
Vec3 snapRenderOrigin(const Vec3& eye)
{
    return eye;
}

} // namespace

void OpenGLVer2Renderer::getFloatingOriginMatrices(
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

unsigned int OpenGLVer2Renderer::cachedOverrideTexture(const std::string& path, bool srgb)
{
    if (path.empty())
        return 0;
    const std::string key = std::string(srgb ? "s:" : "l:") + path;
    auto found = m_overrideTextureCache.find(key);
    if (found != m_overrideTextureCache.end())
        return found->second;
    const GLuint t = loadTexture2DFromFile(path, srgb);
    if (t)
        m_overrideTextureCache[key] = t;
    return t;
}

void OpenGLVer2Renderer::releaseOverrideTextureCache()
{
    for (auto& kv : m_overrideTextureCache) {
        if (kv.second)
            glDeleteTextures(1, &kv.second);
    }
    m_overrideTextureCache.clear();
}

void OpenGLVer2Renderer::resolveStaticMeshPartTextures(
    const StaticMeshPart& part,
    const StaticMeshMaterialOverrideComponent* ov,
    unsigned int& outAlb,
    unsigned int& outN,
    unsigned int& outOcc,
    unsigned int& outMr,
    bool& outUseN,
    bool& outUseOcc,
    bool& outUseMr)
{
    outAlb = part.albedo;
    outN = part.hasNormalMap ? part.normalMap : part.albedo;
    outOcc = part.hasOcclusion ? part.occlusionMap : part.albedo;
    outMr = part.hasMetallicRoughness ? part.metallicRoughnessMap : part.albedo;
    outUseN = part.hasNormalMap;
    outUseOcc = part.hasOcclusion;
    outUseMr = part.hasMetallicRoughness;
    if (!ov)
        return;
    if (!ov->albedoTexturePath.empty()) {
        const unsigned int t = cachedOverrideTexture(ov->albedoTexturePath, true);
        if (t)
            outAlb = t;
    }
    if (!ov->normalTexturePath.empty()) {
        const unsigned int t = cachedOverrideTexture(ov->normalTexturePath, false);
        if (t) {
            outN = t;
            outUseN = true;
        }
    }
    if (!ov->occlusionTexturePath.empty()) {
        const unsigned int t = cachedOverrideTexture(ov->occlusionTexturePath, false);
        if (t) {
            outOcc = t;
            outUseOcc = true;
        }
    }
    if (!ov->metallicRoughnessTexturePath.empty()) {
        const unsigned int t = cachedOverrideTexture(ov->metallicRoughnessTexturePath, false);
        if (t) {
            outMr = t;
            outUseMr = true;
        }
    }
}

void OpenGLVer2Renderer::drawTexturedModel(
    Registry& registry,
    const Mat4& pvShifted,
    const Mat4& tmO,
    const Mat4& model,
    const std::string& assetCacheKey,
    const Vec3& cameraWorld,
    Entity meshEntity)
{
    auto it = m_gpuMeshByAssetKey.find(assetCacheKey);
    if (it == m_gpuMeshByAssetKey.end() || it->second.empty() || m_texProgram == 0)
        return;

    const StaticMeshMaterialOverrideComponent* ov = nullptr;
    if (meshEntity != INVALID_ENTITY && registry.hasComponent<StaticMeshMaterialOverrideComponent>(meshEntity))
        ov = &registry.getComponent<StaticMeshMaterialOverrideComponent>(meshEntity);

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
    bindDisplacementSlotUnused(m_texProgram, m_texWhite1x1);

    const GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (const StaticMeshPart& part : it->second) {
        unsigned int alb = 0;
        unsigned int nmap = 0;
        unsigned int occ = 0;
        unsigned int mr = 0;
        bool useN = false;
        bool useOcc = false;
        bool useMr = false;
        resolveStaticMeshPartTextures(part, ov, alb, nmap, occ, mr, useN, useOcc, useMr);

        glUniform1i(uUseN, useN ? 1 : 0);
        glUniform1i(uUseOcc, useOcc ? 1 : 0);
        glUniform1i(uUseMr, useMr ? 1 : 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, alb);
        glUniform1i(uAlb, 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, nmap);
        glUniform1i(uNorm, 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, occ);
        glUniform1i(uOcc, 2);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, mr);
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

void OpenGLVer2Renderer::drawTexturedSkinnedModel(
    Registry& registry,
    const Mat4& pvShifted,
    const Mat4& tmO,
    const Mat4& model,
    const std::string& assetCacheKey,
    const std::vector<Mat4>& jointSkinMatrices,
    const Vec3& cameraWorld,
    Entity meshEntity)
{
    auto it = m_gpuMeshByAssetKey.find(assetCacheKey);
    if (it == m_gpuMeshByAssetKey.end() || it->second.empty() || m_skinTexProgram == 0)
        return;

    const StaticMeshMaterialOverrideComponent* ov = nullptr;
    if (meshEntity != INVALID_ENTITY && registry.hasComponent<StaticMeshMaterialOverrideComponent>(meshEntity))
        ov = &registry.getComponent<StaticMeshMaterialOverrideComponent>(meshEntity);

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
    bindDisplacementSlotUnused(m_skinTexProgram, m_texWhite1x1);

    const GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (const StaticMeshPart& part : it->second) {
        if (!part.skinned)
            continue;

        unsigned int alb = 0;
        unsigned int nmap = 0;
        unsigned int occ = 0;
        unsigned int mr = 0;
        bool useN = false;
        bool useOcc = false;
        bool useMr = false;
        resolveStaticMeshPartTextures(part, ov, alb, nmap, occ, mr, useN, useOcc, useMr);

        glUniform1i(uUseN, useN ? 1 : 0);
        glUniform1i(uUseOcc, useOcc ? 1 : 0);
        glUniform1i(uUseMr, useMr ? 1 : 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, alb);
        glUniform1i(uAlb, 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, nmap);
        glUniform1i(uNorm, 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, occ);
        glUniform1i(uOcc, 2);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, mr);
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
        bindDisplacementSlotUnused(m_texProgram, m_texWhite1x1);
        for (const StaticMeshPart& part : it->second) {
            if (part.skinned)
                continue;
            unsigned int alb = 0;
            unsigned int nmap = 0;
            unsigned int occ = 0;
            unsigned int mr = 0;
            bool useN = false;
            bool useOcc = false;
            bool useMr = false;
            resolveStaticMeshPartTextures(part, ov, alb, nmap, occ, mr, useN, useOcc, useMr);
            glUniform1i(uUseN, useN ? 1 : 0);
            glUniform1i(uUseOcc, useOcc ? 1 : 0);
            glUniform1i(uUseMr, useMr ? 1 : 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, alb);
            glUniform1i(uAlb, 0);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, nmap);
            glUniform1i(uNorm, 1);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, occ);
            glUniform1i(uOcc, 2);
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, mr);
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

void OpenGLVer2Renderer::shutdown() {
    clearRenderPasses();
    releaseTerrainMeshes();
    releaseOverrideTextureCache();
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
    if (m_boxVbo) {
        glDeleteBuffers(1, &m_boxVbo);
        m_boxVbo = 0;
    }
    if (m_boxVao) {
        glDeleteVertexArrays(1, &m_boxVao);
        m_boxVao = 0;
    }
    m_boxVertexCount = 0;
    if (m_lineVbo) {
        glDeleteBuffers(1, &m_lineVbo);
        m_lineVbo = 0;
    }
    if (m_lineVao) {
        glDeleteVertexArrays(1, &m_lineVao);
        m_lineVao = 0;
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
    if (m_debugHudVbo) {
        glDeleteBuffers(1, &m_debugHudVbo);
        m_debugHudVbo = 0;
    }
    if (m_debugHudVao) {
        glDeleteVertexArrays(1, &m_debugHudVao);
        m_debugHudVao = 0;
    }
    if (m_debugHudProgram) {
        glDeleteProgram(m_debugHudProgram);
        m_debugHudProgram = 0;
        m_debugHudLocFbSize = -1;
    }
    if (m_texWhite1x1) {
        glDeleteTextures(1, &m_texWhite1x1);
        m_texWhite1x1 = 0;
    }
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
        glfwTerminate();
    }
}

void OpenGLVer2Renderer::pollFramebufferSize(int& outW, int& outH) const {
    if (m_window)
        glfwGetFramebufferSize(m_window, &outW, &outH);
    else {
        outW = m_fbW;
        outH = m_fbH;
    }
}

bool OpenGLVer2Renderer::shouldClose() const {
    return m_window && glfwWindowShouldClose(m_window);
}

namespace {

Vec3 terrainVertexNormal(const HeightMapComponent& hm, int x, int z)
{
    const int n = hm.size;
    auto H = [&](int xi, int zi) {
        xi = std::max(0, std::min(n - 1, xi));
        zi = std::max(0, std::min(n - 1, zi));
        return hm.get(xi, zi);
    };
    const float dhx = (H(x + 1, z) - H(x - 1, z)) * 0.5f;
    const float dhz = (H(x, z + 1) - H(x, z - 1)) * 0.5f;
    return normalize({-dhx, 1.0f, -dhz});
}

/// Bitangent sign is +1 for terrain patches (consistent with UV axes ix, iz).
inline Vec3 terrainTangentFromNormal(const Vec3& N)
{
    const Vec3 up{0.0f, 1.0f, 0.0f};
    Vec3 T = cross(up, N);
    if (lengthSquared(T) < 1e-8f)
        return {1.0f, 0.0f, 0.0f};
    return normalize(T);
}

} // namespace

void OpenGLVer2Renderer::releaseTerrainMeshes()
{
    for (auto& kv : m_terrainMeshes) {
        TerrainChunkGpuMesh& g = kv.second;
        if (g.vao)
            glDeleteVertexArrays(1, &g.vao);
        if (g.vbo)
            glDeleteBuffers(1, &g.vbo);
        if (g.ebo)
            glDeleteBuffers(1, &g.ebo);
        g = {};
    }
    m_terrainMeshes.clear();
}

void OpenGLVer2Renderer::syncTerrainMeshes(Registry& registry)
{
    for (auto it = m_terrainMeshes.begin(); it != m_terrainMeshes.end();) {
        if (!registry.hasComponent<TerrainChunkComponent>(it->first)) {
            TerrainChunkGpuMesh& g = it->second;
            if (g.vao)
                glDeleteVertexArrays(1, &g.vao);
            if (g.vbo)
                glDeleteBuffers(1, &g.vbo);
            if (g.ebo)
                glDeleteBuffers(1, &g.ebo);
            it = m_terrainMeshes.erase(it);
        } else
            ++it;
    }

    const PbrMaterialPresetComponent* presetSurf = nullptr;
    float uvRepeats = 4.0f;
    for (Entity te : registry.getEntitiesWith<PbrMaterialPresetComponent>()) {
        const auto& s = registry.getComponent<PbrMaterialPresetComponent>(te);
        if (s.gpuUploaded && s.maps.albedo != 0) {
            presetSurf = &s;
            uvRepeats = s.surfaceUvRepeats;
            break;
        }
    }
    const bool usePbrTerrain = presetSurf != nullptr && m_texProgram != 0;

    for (Entity e : registry.getEntitiesWith<TerrainChunkComponent, HeightMapComponent, WorldTransformComponent>()) {
        if (m_terrainMeshes.count(e) != 0)
            continue;

        const auto& tc = registry.getComponent<TerrainChunkComponent>(e);
        if (tc.skipProceduralTerrainGpuMesh)
            continue;

        const auto& hm = registry.getComponent<HeightMapComponent>(e);
        const float cell = tc.scale;
        const int n = hm.size;
        const int cells = tc.size;

        std::vector<float> interleaved;
        if (usePbrTerrain) {
            interleaved.reserve(static_cast<size_t>(n * n * 12));
            for (int iz = 0; iz < n; ++iz) {
                for (int ix = 0; ix < n; ++ix) {
                    const Vec3 N = terrainVertexNormal(hm, ix, iz);
                    const Vec3 T = terrainTangentFromNormal(N);
                    const float y = hm.get(ix, iz);
                    const float u = (cells > 0) ? static_cast<float>(ix) / static_cast<float>(cells) * uvRepeats : 0.0f;
                    const float v = (cells > 0) ? static_cast<float>(iz) / static_cast<float>(cells) * uvRepeats : 0.0f;
                    interleaved.push_back(static_cast<float>(ix) * cell);
                    interleaved.push_back(y);
                    interleaved.push_back(static_cast<float>(iz) * cell);
                    interleaved.push_back(N.x);
                    interleaved.push_back(N.y);
                    interleaved.push_back(N.z);
                    interleaved.push_back(u);
                    interleaved.push_back(v);
                    interleaved.push_back(T.x);
                    interleaved.push_back(T.y);
                    interleaved.push_back(T.z);
                    interleaved.push_back(1.0f);
                }
            }
        } else {
            interleaved.reserve(static_cast<size_t>(n * n * 6));
            for (int iz = 0; iz < n; ++iz) {
                for (int ix = 0; ix < n; ++ix) {
                    const Vec3 N = terrainVertexNormal(hm, ix, iz);
                    const float y = hm.get(ix, iz);
                    interleaved.push_back(static_cast<float>(ix) * cell);
                    interleaved.push_back(y);
                    interleaved.push_back(static_cast<float>(iz) * cell);
                    interleaved.push_back(N.x);
                    interleaved.push_back(N.y);
                    interleaved.push_back(N.z);
                }
            }
        }

        std::vector<unsigned int> indices;
        indices.reserve(static_cast<size_t>(cells * cells * 6));
        for (int iz = 0; iz < cells; ++iz) {
            for (int ix = 0; ix < cells; ++ix) {
                const unsigned int i00 = static_cast<unsigned int>(iz * n + ix);
                const unsigned int i10 = static_cast<unsigned int>(iz * n + ix + 1);
                const unsigned int i01 = static_cast<unsigned int>((iz + 1) * n + ix);
                const unsigned int i11 = static_cast<unsigned int>((iz + 1) * n + ix + 1);
                // CCW from +Y so front faces point up (back-face cull sees ground from above).
                indices.push_back(i00);
                indices.push_back(i01);
                indices.push_back(i11);
                indices.push_back(i00);
                indices.push_back(i11);
                indices.push_back(i10);
            }
        }

        TerrainChunkGpuMesh gpu{};
        glGenVertexArrays(1, &gpu.vao);
        glGenBuffers(1, &gpu.vbo);
        glGenBuffers(1, &gpu.ebo);
        glBindVertexArray(gpu.vao);
        glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)), interleaved.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);
        if (usePbrTerrain) {
            const GLsizei stride = static_cast<GLsizei>(12 * sizeof(float));
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
            glEnableVertexAttribArray(3);
            gpu.floatsPerVertex = 12;
        } else {
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
            gpu.floatsPerVertex = 6;
        }
        glBindVertexArray(0);
        gpu.indexCount = static_cast<int>(indices.size());
        m_terrainMeshes[e] = gpu;
    }
}

void OpenGLVer2Renderer::drawTerrainMeshes(Registry& registry, const Mat4& pvShifted, const Mat4& tmO, const Vec3& cameraWorld)
{
    if (m_terrainMeshes.empty())
        return;

    const PbrMaterialPresetComponent* surfMat = nullptr;
    for (Entity te : registry.getEntitiesWith<PbrMaterialPresetComponent>()) {
        const auto& s = registry.getComponent<PbrMaterialPresetComponent>(te);
        if (s.gpuUploaded && s.maps.albedo != 0) {
            surfMat = &s;
            break;
        }
    }

    bool layoutPbr = false;
    for (const auto& kv : m_terrainMeshes) {
        if (kv.second.floatsPerVertex == 12) {
            layoutPbr = true;
            break;
        }
    }

    if (surfMat && m_texProgram != 0 && layoutPbr) {
        glUseProgram(m_texProgram);
        const GLint uM = glGetUniformLocation(m_texProgram, "uModel");
        const GLint uMvp = glGetUniformLocation(m_texProgram, "uMVP");
        const GLint uTint = glGetUniformLocation(m_texProgram, "uTint");
        glUniform3f(uTint, 1.0f, 1.0f, 1.0f);
        applyTexturedSceneLighting(m_texProgram, registry, cameraWorld);
        applyHdriUniforms(m_texProgram, registry);
        bindPbrTextureMaps(m_texProgram, surfMat->maps, true);

        const GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const GLboolean polyOffWas = glIsEnabled(GL_POLYGON_OFFSET_FILL);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);

        for (const auto& kv : m_terrainMeshes) {
            Entity e = kv.first;
            if (!registry.hasComponent<WorldTransformComponent>(e))
                continue;
            if (kv.second.floatsPerVertex != 12)
                continue;
            const Mat4& model = registry.getComponent<WorldTransformComponent>(e).world;
            const Mat4 mvp = mat4Mul(pvShifted, mat4Mul(tmO, model));
            glUniformMatrix4fv(uM, 1, GL_FALSE, model.m);
            glUniformMatrix4fv(uMvp, 1, GL_FALSE, mvp.m);
            glBindVertexArray(kv.second.vao);
            glDrawElements(GL_TRIANGLES, kv.second.indexCount, GL_UNSIGNED_INT, nullptr);
        }
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);

        if (!polyOffWas)
            glDisable(GL_POLYGON_OFFSET_FILL);

        if (cullWas)
            glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);

        glUseProgram(m_program);
        return;
    }

    if (m_program == 0)
        return;

    glUseProgram(m_program);
    GLint uM = glGetUniformLocation(m_program, "uModel");
    GLint uMvp = glGetUniformLocation(m_program, "uMVP");
    GLint uC = glGetUniformLocation(m_program, "uColor");
    GLint uL = glGetUniformLocation(m_program, "uLightDir");
    GLint uA = glGetUniformLocation(m_program, "uAmbient");

    const float lightDir[3] = {0.35f, 0.85f, 0.35f};
    float len = std::sqrt(lightDir[0] * lightDir[0] + lightDir[1] * lightDir[1] + lightDir[2] * lightDir[2]);
    float ld[3] = {lightDir[0] / len, lightDir[1] / len, lightDir[2] / len};
    const float amb[3] = {0.18f, 0.2f, 0.24f};

    glUniform3fv(uL, 1, ld);
    glUniform3fv(uA, 1, amb);

    const GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    const GLboolean polyOffWasSolid = glIsEnabled(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);

    for (const auto& kv : m_terrainMeshes) {
        Entity e = kv.first;
        if (!registry.hasComponent<WorldTransformComponent>(e))
            continue;
        if (kv.second.floatsPerVertex != 6)
            continue;
        const Mat4& model = registry.getComponent<WorldTransformComponent>(e).world;
        Mat4 mvp = mat4Mul(pvShifted, mat4Mul(tmO, model));

        float avgH = 2.0f;
        if (registry.hasComponent<HeightMapComponent>(e)) {
            const auto& hm = registry.getComponent<HeightMapComponent>(e);
            float s = 0.f;
            const int nn = hm.size;
            for (int i = 0; i < nn * nn; ++i)
                s += hm.heights[static_cast<size_t>(i)];
            avgH = s / static_cast<float>(nn * nn);
        }
        const float g = 0.02f;
        const float col[3] = {0.22f + g * avgH, 0.48f + g * avgH * 0.5f, 0.18f + g * avgH};

        glUniformMatrix4fv(uM, 1, GL_FALSE, model.m);
        glUniformMatrix4fv(uMvp, 1, GL_FALSE, mvp.m);
        glUniform3fv(uC, 1, col);

        glBindVertexArray(kv.second.vao);
        glDrawElements(GL_TRIANGLES, kv.second.indexCount, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);

    if (!polyOffWasSolid)
        glDisable(GL_POLYGON_OFFSET_FILL);

    if (!cullWas)
        glDisable(GL_CULL_FACE);

    glUseProgram(m_program);
}

void OpenGLVer2Renderer::drawPyramid(const Mat4& mvp, const Mat4& model, const float color[3]) {
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

void OpenGLVer2Renderer::drawBox(const Mat4& mvp, const Mat4& model, const float color[3])
{
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

    glBindVertexArray(m_boxVao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_boxVertexCount));
    glBindVertexArray(0);
}

namespace {

Vec3 worldTranslationFromRegistry(Registry& registry, Entity e)
{
    if (registry.hasComponent<WorldTransformComponent>(e)) {
        const Mat4& w = registry.getComponent<WorldTransformComponent>(e).world;
        return {w.m[12], w.m[13], w.m[14]};
    }
    if (registry.hasComponent<TransformComponent>(e))
        return registry.getComponent<TransformComponent>(e).position;
    return {0.0f, 0.0f, 0.0f};
}

class TerrainRenderPass final : public IRenderPass {
public:
    explicit TerrainRenderPass(OpenGLVer2Renderer* r)
        : m_r(r)
    {
    }
    int sortKey() const override { return 100; }
    void render(RenderContext& ctx) override
    {
        if (!ctx.registry || !m_r)
            return;
        m_r->executeTerrainPass(ctx);
    }

private:
    OpenGLVer2Renderer* m_r = nullptr;
};

class StaticSkinnedMeshRenderPass final : public IRenderPass {
public:
    explicit StaticSkinnedMeshRenderPass(OpenGLVer2Renderer* r)
        : m_r(r)
    {
    }
    int sortKey() const override { return 200; }
    void render(RenderContext& ctx) override
    {
        if (!ctx.registry || !m_r)
            return;
        m_r->executeStaticSkinnedMeshesPass(ctx);
    }

private:
    OpenGLVer2Renderer* m_r = nullptr;
};

class PlayerFallbackRenderPass final : public IRenderPass {
public:
    explicit PlayerFallbackRenderPass(OpenGLVer2Renderer* r)
        : m_r(r)
    {
    }
    int sortKey() const override { return 300; }
    void render(RenderContext& ctx) override
    {
        if (!ctx.registry || !m_r)
            return;
        m_r->executeDebugPlayerFallbackPass(ctx);
    }

private:
    OpenGLVer2Renderer* m_r = nullptr;
};

} // namespace

void OpenGLVer2Renderer::registerRenderPass(std::unique_ptr<IRenderPass> pass)
{
    if (!pass)
        return;
    m_renderPasses.push_back(std::move(pass));
    std::stable_sort(
        m_renderPasses.begin(),
        m_renderPasses.end(),
        [](const std::unique_ptr<IRenderPass>& a, const std::unique_ptr<IRenderPass>& b) {
            return a->sortKey() < b->sortKey();
        });
}

void OpenGLVer2Renderer::clearRenderPasses()
{
    m_renderPasses.clear();
}

// Default pass order (sortKey): terrain (100) → static/skinned meshes (200) → player fallback (300).
// Replace via clearRenderPasses() + registerRenderPass() for custom pipelines (see RenderPipeline.hpp).
void OpenGLVer2Renderer::installDefaultRenderPasses()
{
    clearRenderPasses();
    registerRenderPass(std::make_unique<TerrainRenderPass>(this));
    registerRenderPass(std::make_unique<StaticSkinnedMeshRenderPass>(this));
    registerRenderPass(std::make_unique<PlayerFallbackRenderPass>(this));
}

void OpenGLVer2Renderer::executeTerrainPass(RenderContext& ctx)
{
    if (!ctx.registry)
        return;
    syncTerrainMeshes(*ctx.registry);
    drawTerrainMeshes(*ctx.registry, ctx.pvShifted, ctx.tmO, ctx.cameraWorld);
}

void OpenGLVer2Renderer::executeStaticSkinnedMeshesPass(RenderContext& ctx)
{
    if (!ctx.registry)
        return;
    Registry& registry = *ctx.registry;

    for (Entity e : registry.getEntitiesWith<RenderableMeshComponent, TransformComponent>()) {
        auto& smc = registry.getComponent<RenderableMeshComponent>(e);
        if (!smc.gpuRegistered || smc.assetCacheKey.empty())
            continue;
        if (m_gpuMeshByAssetKey.find(smc.assetCacheKey) == m_gpuMeshByAssetKey.end())
            continue;

        Mat4 base = Mat4::Identity();
        const auto& t = registry.getComponent<TransformComponent>(e);
        if (registry.hasComponent<WorldTransformComponent>(e)) {
            const auto& w = registry.getComponent<WorldTransformComponent>(e).world;
            const Vec3 wp{w.m[12], w.m[13], w.m[14]};
            // TransformSystem only stores translation in `world`; apply local rotation/scale for facing.
            base = Mat4::FromTRS(wp, t.rotation, t.scale);
        } else
            base = Mat4::FromTRS(t.position, t.rotation, t.scale);

        const Mat4 R = Mat4::FromTR({0.0f, 0.0f, 0.0f}, smc.modelSpaceRotation);
        const Mat4 S = Mat4::FromScale({smc.uniformScale, smc.uniformScale, smc.uniformScale});
        const Mat4 Tm = Mat4::FromTranslation(smc.modelSpaceTranslation);
        const Mat4 combined = mat4Mul(mat4Mul(mat4Mul(base, R), S), Tm);

        if (registry.hasComponent<GpuSkinPaletteComponent>(e)) {
            const auto& skinPal = registry.getComponent<GpuSkinPaletteComponent>(e);
            if (!skinPal.jointSkinMatrices.empty())
                drawTexturedSkinnedModel(
                    registry,
                    ctx.pvShifted,
                    ctx.tmO,
                    combined,
                    smc.assetCacheKey,
                    skinPal.jointSkinMatrices,
                    ctx.cameraWorld,
                    e);
            else
                drawTexturedModel(
                    registry, ctx.pvShifted, ctx.tmO, combined, smc.assetCacheKey, ctx.cameraWorld, e);
        } else {
            drawTexturedModel(
                registry, ctx.pvShifted, ctx.tmO, combined, smc.assetCacheKey, ctx.cameraWorld, e);
        }
    }

    glUseProgram(m_program);
    for (Entity e : registry.getEntitiesWith<PrimitivePyramidComponent, TransformComponent>()) {
        const auto& p = registry.getComponent<PrimitivePyramidComponent>(e);
        const auto& t = registry.getComponent<TransformComponent>(e);
        const Mat4 model = Mat4::FromTRS(t.position, t.rotation, {p.scale, p.scale, p.scale});
        const Mat4 mvp = mat4Mul(ctx.pvShifted, mat4Mul(ctx.tmO, model));
        drawPyramid(mvp, model, p.color);
    }
    for (Entity e : registry.getEntitiesWith<PrimitiveBoxComponent, TransformComponent>()) {
        const auto& box = registry.getComponent<PrimitiveBoxComponent>(e);
        const auto& t = registry.getComponent<TransformComponent>(e);
        const Mat4 model = Mat4::FromTRS(
            t.position,
            t.rotation,
            {box.halfExtents.x * 2.f, box.halfExtents.y * 2.f, box.halfExtents.z * 2.f});
        const Mat4 mvp = mat4Mul(ctx.pvShifted, mat4Mul(ctx.tmO, model));
        drawBox(mvp, model, box.color);
    }
    drawDebugRaycasts(ctx);
    glUseProgram(m_texProgram);
}

void OpenGLVer2Renderer::executeDebugPlayerFallbackPass(RenderContext& ctx)
{
    if (!ctx.registry)
        return;
    Registry& registry = *ctx.registry;

    glUseProgram(m_program);

    Entity playerEntity = INVALID_ENTITY;
    for (Entity e : registry.getEntitiesWith<PlayerTagComponent, TransformComponent>()) {
        playerEntity = e;
        break;
    }

    auto drawAt = [&](const Vec3& pos, float scale, const float col[3]) {
        Mat4 model = mat4Mul(Mat4::FromTranslation(pos), Mat4::FromScale({scale, scale, scale}));
        Mat4 mvp = mat4Mul(ctx.pvShifted, mat4Mul(ctx.tmO, model));
        drawPyramid(mvp, model, col);
    };

    const float colPlayer[3] = {0.2f, 0.75f, 0.35f};

    bool playerHasRenderableMesh = false;
    if (playerEntity != INVALID_ENTITY && registry.hasComponent<RenderableMeshComponent>(playerEntity)) {
        const auto& sm = registry.getComponent<RenderableMeshComponent>(playerEntity);
        playerHasRenderableMesh =
            sm.gpuRegistered && !sm.assetCacheKey.empty() &&
            m_gpuMeshByAssetKey.find(sm.assetCacheKey) != m_gpuMeshByAssetKey.end();
    }

    if (playerEntity != INVALID_ENTITY && !playerHasRenderableMesh)
        drawAt(worldTranslationFromRegistry(registry, playerEntity), 1.15f, colPlayer);
}

void OpenGLVer2Renderer::renderFrame(Registry& registry) {
    if (!m_window || !m_program)
        return;

    glfwPollEvents();

    glfwGetFramebufferSize(m_window, &m_fbW, &m_fbH);
    if (m_fbW <= 0 || m_fbH <= 0) {
        int ww = 0, wh = 0;
        glfwGetWindowSize(m_window, &ww, &wh);
        m_fbW = std::max(1, ww);
        m_fbH = std::max(1, wh);
    }

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
    if (cameraEntity == INVALID_ENTITY) {
        drawDebugHudOverlay();
        glfwSwapBuffers(m_window);
        return;
    }

    auto& cam = registry.getComponent<CameraComponent>(cameraEntity);

    const Vec3 eye = worldTranslationFromRegistry(registry, cameraEntity);

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

    RenderContext ctx;
    ctx.registry     = &registry;
    ctx.renderer     = static_cast<IGraphicsRenderer*>(this);
    ctx.pvShifted    = pvShifted;
    ctx.tmO          = tmO;
    ctx.cameraWorld  = eye;

    if (m_renderPasses.empty())
        installDefaultRenderPasses();

    for (auto& pass : m_renderPasses) {
        if (pass)
            pass->render(ctx);
    }

    drawDebugHudOverlay();

    glfwSwapBuffers(m_window);
}
