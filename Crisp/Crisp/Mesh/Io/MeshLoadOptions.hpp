#pragma once

namespace crisp {

struct TriangleMeshLoadOptions {
    bool normalizeNormals{true};     // If normals are available, they will be renormalized.
    bool computeVertexNormals{true}; // If normals are not available, they will be computed on load.
    bool computeTangents{true};      // If tangents are not available, they will be computed on load.

    bool optimizeIndices{true};
};

} // namespace crisp
