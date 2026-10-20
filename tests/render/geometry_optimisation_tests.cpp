#include "rendering/exact-indexed-mesh.h"
#include "rendering/terrain/terrain-paint-coverage.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>

struct Batch {
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    bool alphaLayer = true, ready = true;
};
Batch Triangle(float alpha, float offset = 0.0f) {
    Batch b;
    for (int i = 0; i < 3; ++i) {
        const float vertex[12]{offset + (i == 1), 0.0f, float(i == 2),
            0, 1, 0, float(i == 1), float(i == 2), alpha, 1, 1, 1};
        b.vertices.insert(b.vertices.end(), vertex, vertex + 12);
    }
    return b;
}
size_t Prune(std::vector<Batch>& b) {
    return fo3terrain::RemoveCoveredPaint(b, [](const Batch& batch) { return batch.ready; });
}
std::vector<float> Expanded(const Batch& b) {
    if (b.indices.empty()) return b.vertices;
    std::vector<float> result;
    for (uint32_t i : b.indices) result.insert(result.end(), b.vertices.begin() + i * 12u,
                                              b.vertices.begin() + (i + 1u) * 12u);
    return result;
}
int main() {
    // Indexed expansion reconstructs *every bit* of all original attributes.
    Batch b = Triangle(0.5f);
    const auto triangle = b.vertices;
    for (int i = 0; i < 12; ++i) b.vertices.insert(b.vertices.end(), triangle.begin(), triangle.end());
    // A UV seam, normal seam, colour seam and alpha seam must each remain distinct.
    b.vertices[3 * 12 + 6] += 1.0f;
    b.vertices[6 * 12 + 4] = 0.75f;
    b.vertices[9 * 12 + 9] = 0.25f;
    b.vertices[12 * 12 + 8] = 0.75f;
    b.vertices[15 * 12 + 0] = -0.0f;
    const auto original = b.vertices;
    assert(fqmesh::IndexExact<12>(b.vertices, b.indices));
    const auto restored = Expanded(b);
    assert(restored.size() == original.size());
    assert(std::memcmp(restored.data(), original.data(), original.size() * sizeof(float)) == 0);
    assert(b.vertices.size() * sizeof(float) + b.indices.size() * sizeof(uint32_t) < original.size() * sizeof(float));
    // Non-repeating/small and malformed streams retain the original array path.
    auto tiny = Triangle(1);
    auto tinyOriginal = tiny.vertices;
    assert(!fqmesh::IndexExact<12>(tiny.vertices, tiny.indices));
    assert(tiny.vertices == tinyOriginal && tiny.indices.empty());
    tiny.vertices.push_back(1);
    assert(!fqmesh::IndexExact<12>(tiny.vertices, tiny.indices));

    Batch base = Triangle(1); base.alphaLayer = false;
    std::vector<Batch> layers{base, Triangle(0.3f), Triangle(1)};
    assert(Prune(layers) == 1 && !layers[0].vertices.empty());
    assert(layers[1].vertices.empty() && !layers[2].vertices.empty());
    layers = {base, Triangle(0.3f), Triangle(0.99999f)};
    assert(Prune(layers) == 0); // Never approximate full opacity.
    layers[2].vertices[8] = 1; layers[2].vertices[20] = 1;
    assert(Prune(layers) == 0); // One partial vertex still exposes lower paint.
    layers = {base, Triangle(0.3f), Triangle(1)}; layers[2].ready = false;
    assert(Prune(layers) == 0); // A missing/capped/failed diffuse cannot cover anything.
    layers = {base, Triangle(0.3f), Triangle(1, 0.0001f)};
    assert(Prune(layers) == 0); // No geometric tolerance that could punch holes.
    layers = {base, Triangle(0)};
    assert(Prune(layers) == 1 && !layers[0].vertices.empty());

    // Triangle order inside a batch matters too: keep an opaque triangle and
    // a *later* translucent triangle, but remove an earlier covered one.
    Batch mixed = Triangle(1); auto partial = Triangle(0.5f).vertices;
    mixed.vertices.insert(mixed.vertices.end(), partial.begin(), partial.end());
    layers = {base, mixed}; assert(Prune(layers) == 0);
    mixed = Triangle(0.5f); auto full = Triangle(1).vertices;
    mixed.vertices.insert(mixed.vertices.end(), full.begin(), full.end());
    layers = {base, mixed}; assert(Prune(layers) == 1);

    // Compare actual ordered RGB + alpha blending at barycentric samples,
    // including compact indexed streams, for deterministic varied layers.
    std::mt19937 random(237);
    for (int iteration = 0; iteration < 400; ++iteration) {
        layers = {base};
        std::vector<float> colours;
        for (int layer = 0; layer < 8; ++layer) {
            Batch paint = Triangle(float(random() % 5) * 0.25f);
            for (int v = 0; v < 3; ++v) paint.vertices[v * 12 + 9] = float(random() % 100) / 50.0f;
            auto copy = paint.vertices;
            paint.vertices.insert(paint.vertices.end(), copy.begin(), copy.end());
            assert(fqmesh::IndexExact<12>(paint.vertices, paint.indices));
            layers.push_back(std::move(paint));
        }
        auto composite = [](const std::vector<Batch>& batches) {
            double rgb = 0, alpha = 1;
            for (const Batch& batch : batches) {
                if (!batch.ready) continue;
                auto data = Expanded(batch);
                for (size_t i = 0; i < data.size(); i += 36) {
                    const double a = (data[i + 8] + data[i + 20] + data[i + 32]) / 3.0;
                    const double colour = (data[i + 9] + data[i + 21] + data[i + 33]) / 3.0;
                    rgb = colour * a + rgb * (1 - a);
                    alpha = a * a + alpha * (1 - a);
                }
            }
            return std::array<double, 2>{rgb, alpha};
        };
        const auto before = composite(layers);
        Prune(layers);
        const auto after = composite(layers);
        assert(std::fabs(before[0] - after[0]) < 1e-12 && std::fabs(before[1] - after[1]) < 1e-12);
    }
    std::cout << "Exact geometry indexing, seams, ordered paint coverage and compositing passed\n";
}
