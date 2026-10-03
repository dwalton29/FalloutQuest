# Original loading slideshow playback

The loading renderer reads the original `Interface/Loading/LoadingAnim01.NIF` from the installed game data, including its hierarchy, three embedded transform sequences (`Left`, `Forward`, `Backward`) and text keys. It preserves node transforms, linear and quadratic translation/scale keys, XYZ rotation keys, vertex colours, original texture paths and material blend factors.

The projector fits its original background plane to the existing 2.4m-wide panel, 2.5m from the player. Off-screen parked slides do not affect sizing or centring. Layers retain their authored depth offsets, render from back to front, and clip to the background rectangle. A missing or unsupported animated overlay falls back to the selected artwork.

A loading generation plays `Left`, then alternates `Forward` and `Backward` as loading continues. The six-second hold between changes and automatic direction selection are VR presentation choices; the supplied assets do not establish the original engine's scheduling logic. Movement durations and sound-key timing come from the NIF. The original sound EDIDs are resolved through the audio catalogue and original Sound BSA; missing sound data remains silent.

Up to four unique, location-eligible LSCR artworks are decoded on the preparation worker with a 16MiB deck limit. Textures upload one per submitted frame, then the two authored slide slots change without further file reads or uploads. A single available artwork still receives the original introduction, without unnecessary repeated slide changes. Both eyes use one pose and sound-event update per frame.

The circular loading compass decodes its separate `Idle` sequence: the dial remains fixed while the pointer animates independently. Its authored loop and node hierarchy are preserved. Slide textures flip their V coordinates to present the DDS artwork upright. The rotating exhibit remains the original model in the foreground.

Validation: synthetic regression tests run in the existing NIF CTest suite. Optional original-asset tests decode three clips, bind eighteen tracks and eight textured shapes, sample 3,600 slideshow frames, verify the intro finishes at the panel centre, and reject truncated files. A Quest device is required to verify the final stereo appearance and audio.

Audio lookup accepts case-insensitive Sound/Music folders in either `Fallout3/Data` or `Fallout3`, and the original Sound BSA or sound/sounds.bsa aliases. Startup and playback logs distinguish missing files, JNI bridge failure, focus/lifecycle state and decoder failure. Original audio files must still be installed separately; they are not bundled in the APK.
