#pragma once
#include "rendering/exact-indexed-mesh.h"
#include <algorithm>
#include <unordered_set>

namespace fo3terrain {
// Batches must be in the actual authored draw order. Only paint (never the
// depth-writing base) is removed. A cover must have a GPU-ready diffuse map,
// bit-identical triangle positions and alpha exactly one at all vertices.
// Partial alpha, missing textures and uncertain geometry stay on the old path.
template<class Batches, class TextureReady>
size_t RemoveCoveredPaint(Batches& batches, TextureReady textureReady) {
    using Key = std::array<uint32_t, 9>;
    std::unordered_set<Key, fqmesh::VertexHash<9>> opaque;
    size_t removed = 0;
    for (auto batch = batches.rbegin(); batch != batches.rend(); ++batch) {
        if (!batch->alphaLayer || !textureReady(*batch)) continue;
        const bool indexed = !batch->indices.empty();
        const size_t corners = indexed ? batch->indices.size() : batch->vertices.size() / 12u;
        std::vector<uint8_t> keep(corners / 3u, 1u);
        for (size_t triangle = corners / 3u; triangle > 0; --triangle) {
            const size_t first = (triangle - 1u) * 3u;
            Key key{};
            bool full = true, zero = true;
            for (size_t c = 0; c < 3; ++c) {
                const size_t vertex = indexed ? batch->indices[first + c] : first + c;
                const float* data = batch->vertices.data() + vertex * 12u;
                std::memcpy(key.data() + c * 3u, data, 3u * sizeof(float));
                full = full && data[8] == 1.0f;
                zero = zero && data[8] == 0.0f;
            }
            if (zero || opaque.count(key)) { ++removed; keep[triangle - 1u] = 0u; continue; }
            if (full) opaque.insert(key);
        }
        size_t write = 0;
        for (size_t first = 0; first + 2 < corners; first += 3) {
            if (!keep[first / 3u]) continue;
            if (indexed) {
                for (size_t c = 0; c < 3; ++c) batch->indices[write + c] = batch->indices[first + c];
            } else if (write != first) {
                std::memmove(batch->vertices.data() + write * 12u,
                             batch->vertices.data() + first * 12u, 36u * sizeof(float));
            }
            write += 3;
        }
        if (indexed) batch->indices.resize(write);
        else batch->vertices.resize(write * 12u);
        if (write == 0) { batch->vertices.clear(); batch->indices.clear(); }
    }
    return removed;
}
} // namespace fo3terrain
