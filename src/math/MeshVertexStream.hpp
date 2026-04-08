#pragma once

#include "Vertex.hpp"
#include "VertexBoneData.hpp"

/// Interleaved layout for GPU upload (matches attribute order in OpenGLVer2Renderer).
struct MeshVertexStream {
    Vertex vertex;
    VertexBoneData bone;
};
