# Loading NIF material checks

Run `cmake -S tests/nif -B build/host-nif`, then
`cmake --build build/host-nif` and
`ctest --test-dir build/host-nif --output-on-failure`.

The regression test covers TileShaderProperty filenames, truncation and malformed
lengths. To verify complete geometry/material decoding against extracted original
game assets, pass paths to `loadinganim01.nif` and `loading01.nif` as arguments to
`build/host-nif/loading_material_tests`. Original assets are not included.


Actor tests additionally exercise skeleton/clip sampling and NPC record assembly.
Optional local integration (user-supplied assets only):

```sh
./build/actor_animation_tests /path/to/skeleton.nif /path/to/mtidle.kf /path/to/mtforward.kf
./build/npc_record_tests /path/to/Fallout3.esm
```

These inputs are never committed or needed for the synthetic CI tests.

Loading slideshow coverage (`loading_animation_tests`) checks embedded UI hierarchy binding, all sequences, text/sound key timing, two-slot changes, single-image handling, restart and long-frame behavior. Pass an original `loadinganim01.nif` path for asset integration; it is not shipped in the repository.
