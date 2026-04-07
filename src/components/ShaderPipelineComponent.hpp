#pragma once

// -----------------------------------------------------------------------------
// Selects which built-in shader path draws an entity (extensible enum).
// Render passes may read this to choose programs / feature flags.
// -----------------------------------------------------------------------------

enum class ShaderPipelineId : int {
    DefaultTextured = 0,
    SkinnedTextured = 1,
    UnlitVertexColor = 2,
};

struct ShaderPipelineComponent {
    ShaderPipelineId pipeline = ShaderPipelineId::DefaultTextured;
};
