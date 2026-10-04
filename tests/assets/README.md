# Shared archive tests

Run without Android or Bethesda assets:

```sh
cmake -S tests/assets -B build/host-assets
cmake --build build/host-assets
ctest --test-dir build/host-assets --output-on-failure
```

The generated fixtures cover compressed/uncompressed entries, both archive
compression defaults, per-entry overrides, embedded filenames, mesh/texture
root aliases, sorted prefix probes, size limits, malformed/truncated records,
atomic index publication and concurrent registry acquisition/extraction.

Dependencies: a C++17 compiler, CMake, zlib development headers and threads.
The APK workflow runs these checks before building the Android application.

The portable `font_layout_tests` covers synthetic Fallout FNT structure and
baseline/advance/whitespace layout. To validate your own original without
embedding assets, run `build/host-assets/font_layout_tests
/path/to/Data/textures/fonts/baked-in_monofonto_large.fnt` (one command line).
See [font recovery notes](../../docs/FALLOUT-FONT-LAYOUT.md) for executable
addresses, consumer audit, exact rules, diagnostics and missing-input limits.
