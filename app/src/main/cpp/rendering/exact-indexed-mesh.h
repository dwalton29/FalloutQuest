#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <vector>

namespace fqmesh {
// Compare the entire vertex bit-for-bit: never weld normal, colour, UV or
// alpha seams. First occurrence and triangle order are preserved.
template<size_t Width> struct VertexHash {
    size_t operator()(const std::array<uint32_t, Width>& key) const {
        size_t hash = 2166136261u;
        for (uint32_t word : key) hash = (hash ^ word) * 16777619u;
        return hash;
    }
};
template<size_t Width>
bool IndexExact(std::vector<float>& vertices, std::vector<uint32_t>& indices) {
    static_assert(sizeof(float) == sizeof(uint32_t));
    indices.clear();
    if (vertices.empty() || vertices.size() % (Width * 3u) != 0u) return false;
    const size_t count = vertices.size() / Width;
    if (count > std::numeric_limits<uint32_t>::max()) return false;
    std::unordered_map<std::array<uint32_t, Width>, uint32_t, VertexHash<Width>> seen;
    std::vector<float> unique;
    std::vector<uint32_t> order;
    seen.reserve(count);
    unique.reserve(vertices.size());
    order.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        std::array<uint32_t, Width> key;
        std::memcpy(key.data(), vertices.data() + i * Width, sizeof(key));
        const auto entry = seen.emplace(key, static_cast<uint32_t>(unique.size() / Width));
        if (entry.second) unique.insert(unique.end(), vertices.begin() + i * Width,
                                       vertices.begin() + (i + 1u) * Width);
        order.push_back(entry.first->second);
    }
    if (unique.size() * sizeof(float) + order.size() * sizeof(uint32_t) >=
        vertices.size() * sizeof(float)) return false;
    vertices = std::move(unique);
    indices = std::move(order);
    return true;
}
} // namespace fqmesh
