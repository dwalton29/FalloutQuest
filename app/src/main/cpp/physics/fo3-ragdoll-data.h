#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace fo3rag {
using V=std::array<float,3>;
using M=std::array<float,16>;
struct Body {
 std::string bone;
 V a{},b{},center{};
 float radius=0,mass=0,linearDamping=0,angularDamping=0,friction=0,restitution=0;
 std::array<float,9> inertia{};
};
struct Joint {
 uint16_t a=0,b=0;bool hinge=false;
 // Local NIF units. Column-major orthonormal frames; pivots retain authored scale.
 M frameA{},frameB{};
 V minimum{},maximum{};
 float friction=0,strength=1;
};
struct Definition {std::vector<Body> bodies;std::vector<Joint> joints;};
// First checkpoint: FO3 capsule skeletons, limited hinges and ragdoll joints,
// including malleable wrappers. Unsupported graphs fail as a whole.
bool Read(const std::vector<uint8_t>& bytes,Definition& out,std::string& error);
}
