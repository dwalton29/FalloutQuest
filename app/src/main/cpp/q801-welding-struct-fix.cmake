# Q8.1 build fix: Q7.23 already added walkableModuleQ723 to CollisionTriangle,
# so add the Q8.1 welding fields after that existing member rather than matching
# the older stairs/platform tail.
string(REPLACE
    "    bool walkableModuleQ723 = false;\n};"
    "    bool walkableModuleQ723 = false;\n\n    // Q8.1: packed Havok welding and exact source-mesh edge identity.\n    uint16_t weldingInfoQ801 = 0u;\n    uint64_t meshKeyQ801 = 0u;\n    uint32_t vertexAQ801 = 0xffffffffu;\n    uint32_t vertexBQ801 = 0xffffffffu;\n    uint32_t vertexCQ801 = 0xffffffffu;\n};"
    Q74_COLLISION_SOURCE "${Q74_COLLISION_SOURCE}")
