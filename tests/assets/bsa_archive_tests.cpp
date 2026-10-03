#include "fo3-asset-store.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <zlib.h>

using Bytes = std::vector<uint8_t>;
using fo3assets::BsaArchive;
using fo3assets::BsaFileInfo;
using fo3assets::BsaPathKind;

namespace {

void Check(bool result, const char* message) {
    if (!result) throw std::runtime_error(message);
}
void U32(Bytes& out, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) out.push_back(uint8_t(value >> (i * 8)));
}
void SetU32(Bytes& out, size_t offset, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) out.at(offset + i) = uint8_t(value >> (i * 8));
}
void Text(Bytes& out, const std::string& text) {
    out.insert(out.end(), text.begin(), text.end());
}

struct FixtureFile { std::string name; Bytes bytes; bool toggle = false; };

// Tiny generated BSA fixtures, containing no game assets. Deliberately cover
// both archive compression defaults and the per-file XOR override.
Bytes MakeBsa(uint32_t flags, const std::string& folder,
              const std::vector<FixtureFile>& files) {
    Bytes out{'B', 'S', 'A', 0};
    U32(out, 104); U32(out, 36); U32(out, flags);
    U32(out, 1); U32(out, static_cast<uint32_t>(files.size()));
    U32(out, static_cast<uint32_t>(folder.size() + 1));
    uint32_t names = 0;
    for (const auto& file : files) names += static_cast<uint32_t>(file.name.size() + 1);
    U32(out, names); U32(out, 0);
    U32(out, 0); U32(out, 0); U32(out, static_cast<uint32_t>(files.size()));
    U32(out, 52 + names);
    out.push_back(static_cast<uint8_t>(folder.size() + 1));
    Text(out, folder); out.push_back(0);
    const size_t records = out.size();
    out.resize(records + files.size() * 16u);
    for (const auto& file : files) { Text(out, file.name); out.push_back(0); }
    for (size_t i = 0; i < files.size(); ++i) {
        const auto& file = files[i];
        Bytes payload;
        if ((flags & 0x100u) != 0u) {
            const std::string embedded = folder + "\\" + file.name;
            payload.push_back(static_cast<uint8_t>(embedded.size()));
            Text(payload, embedded);
        }
        if (((flags & 4u) != 0u) != file.toggle) {
            U32(payload, static_cast<uint32_t>(file.bytes.size()));
            uLongf length = compressBound(static_cast<uLong>(file.bytes.size()));
            Bytes packed(length);
            Check(compress(packed.data(), &length, file.bytes.data(),
                           static_cast<uLong>(file.bytes.size())) == Z_OK, "fixture compression");
            packed.resize(length);
            payload.insert(payload.end(), packed.begin(), packed.end());
        } else {
            payload.insert(payload.end(), file.bytes.begin(), file.bytes.end());
        }
        SetU32(out, records + i * 16u + 8u,
               static_cast<uint32_t>(payload.size()) | (file.toggle ? 0x40000000u : 0u));
        SetU32(out, records + i * 16u + 12u, static_cast<uint32_t>(out.size()));
        out.insert(out.end(), payload.begin(), payload.end());
    }
    return out;
}

struct TempDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("falloutquest-bsa-tests-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    TempDirectory() { std::filesystem::create_directories(path); }
    ~TempDirectory() { std::filesystem::remove_all(path); }
    std::string Write(const std::string& name, const Bytes& bytes) {
        const auto file = path / name;
        std::ofstream stream(file, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        Check(bool(stream), "fixture write");
        return file.string();
    }
};

void ExtractionTests(TempDirectory& temp) {
    const Bytes first{'N', 'I', 'F', 0, 1, 255};
    const Bytes second(256, 42);
    for (uint32_t flags : {3u, 7u, 0x103u, 0x107u}) {
        const std::string path = temp.Write(std::to_string(flags) + ".bsa",
            MakeBsa(flags, "Meshes\\Test", {{"A.NIF", first, false}, {"B.NIF", second, true}}));
        BsaArchive archive(path);
        Bytes bytes{123};
        BsaFileInfo info;
        Check(archive.Read("/MESHES/Test/A.NIF", bytes, &info), "case/slash normalization");
        Check(bytes == first && info.path == "meshes\\test\\a.nif", "first payload and path");
        Check(info.compressed == ((flags & 4u) != 0u), "archive compression default");
        Check(archive.Read("test/b.nif", bytes, &info, BsaPathKind::Mesh), "mesh root alias");
        Check(bytes == second && info.compressed == ((flags & 4u) == 0u), "XOR compression toggle");
        Check(!archive.Read("test/missing.nif", bytes, &info, BsaPathKind::Mesh), "missing file");
        Check(bytes.empty() && info.path.empty(), "failure outputs cleared");
        Check(!archive.Read("meshes/test/b.nif", bytes, nullptr, BsaPathKind::Exact, 32), "size limit");
        Check(bytes.empty(), "size failure cleared");
        std::vector<BsaFileInfo> listed;
        Check(archive.List("test/", listed, BsaPathKind::Mesh) && listed.size() == 2, "mesh prefix alias");
        Check(archive.List("", listed, BsaPathKind::Exact, 1) && listed.size() == 1 &&
              listed.front().path == "meshes\\test\\a.nif", "sorted limited listing");
    }
    for (const std::string folder : {"Textures\\UI", "UI", "Data\\Textures\\UI"}) {
        const auto path = temp.Write("textures-" + std::to_string(folder.size()) + ".bsa",
                                    MakeBsa(3, folder, {{"FONT.FNT", first, false}}));
        BsaArchive archive(path);
        for (const char* request : {"ui/font.fnt", "Textures/UI/font.fnt", "Data/Textures/UI/font.fnt"}) {
            Bytes bytes;
            Check(archive.Read(request, bytes, nullptr, BsaPathKind::Texture) && bytes == first,
                  "texture Data/root/bare aliases");
        }
    }
    const auto bare = temp.Write("bare.bsa", MakeBsa(3, "test", {{"a.nif", first, false}}));
    Bytes bytes;
    BsaArchive archive(bare);
    Check(archive.Read("meshes/test/a.nif", bytes, nullptr, BsaPathKind::Mesh) && bytes == first,
          "root request for rootless mesh archive");
}

void MalformedTests(TempDirectory& temp) {
    const Bytes valid = MakeBsa(7, "x", {{"a", Bytes(64, 10), false}});
    const size_t record = 52u + 1u + 2u;
    std::vector<Bytes> bad;
    bad.push_back(valid); bad.back().resize(20);
    bad.push_back(valid); bad.back()[0] = 'X';
    bad.push_back(valid); SetU32(bad.back(), 4, 105);
    bad.push_back(valid); SetU32(bad.back(), 12, 0);
    bad.push_back(valid); SetU32(bad.back(), 16, 1000001);
    bad.push_back(valid); SetU32(bad.back(), 20, 3000001);
    bad.push_back(valid); SetU32(bad.back(), 44, 2);
    bad.push_back(valid); SetU32(bad.back(), record + 12, 0xffffffffu);
    bad.push_back(valid); bad.back().pop_back();
    bad.push_back(valid); bad.back().back() ^= 0xff;
    // Declared uncompressed output must match zlib output and stay bounded.
    const size_t payload = record + 16u + 2u;
    bad.push_back(valid); SetU32(bad.back(), payload, 63);
    bad.push_back(valid); SetU32(bad.back(), payload, 0xffffffffu);
    // Embedded-name length cannot run past the stored entry.
    bad.push_back(MakeBsa(0x103, "x", {{"a", Bytes{1}, false}}));
    bad.back()[payload] = 255;
    for (size_t i = 0; i < bad.size(); ++i) {
        BsaArchive archive(temp.Write("bad-" + std::to_string(i) + ".bsa", bad[i]));
        Bytes bytes{1, 2}; BsaFileInfo info; info.path = "stale";
        Check(!archive.Read("x/a", bytes, &info), "malformed entry rejected");
        Check(bytes.empty() && info.path.empty(), "malformed failure outputs cleared");
    }
    // A partial index must not survive an invalid second record.
    auto partial = MakeBsa(3, "x", {{"a", Bytes{1}, false}, {"b", Bytes{2}, false}});
    SetU32(partial, record + 16u + 12u, 0xffffffffu);
    BsaArchive archive(temp.Write("partial.bsa", partial));
    std::vector<BsaFileInfo> entries;
    Check(!archive.List("", entries) && entries.empty(), "atomic index publication");
}

void ConcurrencyTests(TempDirectory& temp) {
    const Bytes expected(4096, 17);
    const auto path = temp.Write("concurrent.bsa", MakeBsa(0x107, "textures\\ui",
                                {{"font.fnt", expected, false}}));
    auto archive = fo3assets::GetBsaArchive(path);
    std::atomic<bool> ok{true};
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) threads.emplace_back([&]() {
        for (int n = 0; n < 32; ++n) {
            Bytes bytes;
            if (fo3assets::GetBsaArchive(path) != archive ||
                !archive->Read("ui/font.fnt", bytes, nullptr, BsaPathKind::Texture) ||
                bytes != expected) ok = false;
        }
    });
    for (auto& thread : threads) thread.join();
    Check(ok, "shared index publication and parallel reads");
}

} // namespace

int main() {
    try {
        TempDirectory temp;
        ExtractionTests(temp); MalformedTests(temp); ConcurrencyTests(temp);
        std::cout << "BSA archive extraction, aliases, bounds and concurrency passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
