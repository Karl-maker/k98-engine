#pragma once

#include <string>

// -----------------------------------------------------------------------------
// Selects which built-in shader path draws an entity (extensible enum).
// Optional project-root-relative paths override the enum when both are non-empty
// (backend must compile/link; see OpenGLVer2Renderer).
// -----------------------------------------------------------------------------

enum class ShaderPipelineId : int {
    DefaultTextured = 0,
    SkinnedTextured = 1,
    UnlitVertexColor = 2,
};

struct ShaderPipelineComponent {
    ShaderPipelineId pipeline = ShaderPipelineId::DefaultTextured;

    std::string customVertPath;
    std::string customFragPath;
};
