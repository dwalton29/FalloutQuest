#include "fo3-install-paths.h"
#include "fo3-asset-store.h"
#include <cassert>
#include <fstream>
#include <unistd.h>

int main() {
    char directory[] = "/tmp/fq-install-XXXXXX";
    assert(mkdtemp(directory));
    const std::string root(directory);
    assert(setenv("FALLOUTQUEST_DATA_ROOT", (root + "///").c_str(), 1) == 0);
    std::ofstream(root + "/fallout3.ESM") << "TES4";
    std::ofstream(root + "/FALLOUT - MESHES.BSA") << "BSA";
    assert(fo3assets::FalloutDataPath("") == root + '/');
    assert(fo3assets::FalloutMasterPath() == root + "/fallout3.ESM");
    assert(fo3assets::FalloutDataPath("Fallout - Meshes.bsa") == root + "/FALLOUT - MESHES.BSA");
    assert(fo3assets::FalloutDataPath("missing.bsa") == root + "/missing.bsa");
    // Environment changes cannot swap an installation underneath cached indexes.
    assert(setenv("FALLOUTQUEST_DATA_ROOT", "/another-install", 1) == 0);
    assert(fo3assets::FalloutDataRoot() == root + '/');
    unlink((root + "/fallout3.ESM").c_str());
    unlink((root + "/FALLOUT - MESHES.BSA").c_str());
    rmdir(directory);
}
