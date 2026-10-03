# Loading NIF material checks

Run `cmake -S tests/nif -B build/host-nif`, then
`cmake --build build/host-nif` and
`ctest --test-dir build/host-nif --output-on-failure`.

The regression test covers TileShaderProperty filenames, truncation and malformed
lengths. To verify complete geometry/material decoding against extracted original
game assets, pass paths to `loadinganim01.nif` and `loading01.nif` as arguments to
`build/host-nif/loading_material_tests`. Original assets are not included.
