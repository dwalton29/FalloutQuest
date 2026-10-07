# JNI dialogue completion and animation-root grounding — version 178

Quest testing of v177 identified two remaining headset failures.

## Dialogue crash

The device log reaches the Java audio completion callback and then throws
`UnsatisfiedLinkError` resolving
`FalloutNativeActivity.audioDialogueDone(int, boolean)`. The conventional JNI
symbol is present in the packaged native library, so this is a Java/native
binding problem rather than a dialogue graph, choice panel, or MediaPlayer
timing failure.

The audio runtime now explicitly registers `audioDialogueDone(IZ)V` and
`audioBroadcastDone(II)V` against the live `FalloutNativeActivity` class from
the existing `fo3audio::Start` JNI environment. Conventional exported JNI
symbols remain as fallback diagnostics. This removes dependence on ART
associating NativeActivity's framework-loaded library with the Java subclass.

## NPC feet/root grounding

Fallout locomotion KFs carry an authored accumulation root in addition to the
NonAccum skeletal animation. FalloutQuest already drives world-space actor
motion itself, but the sampler reset the accumulation root to identity and then
reapplied the KF track for that same bone. That could move the rendered skeleton
again after the runtime root had been grounded.

Animation sampling now excludes the accumulation-root track entirely, and
animation blending explicitly preserves the same identity-root invariant.
Child/NonAccum authored motion remains intact.

NAVM still supplies route topology and now also supplies the vertical
neighbourhood used to select the correct stacked physical surface. Exact actor
height comes from resident Havok/LAND grounding; the query is no longer seeded
from a previously erroneous actor Y.

Regression tests cover an explicit accumulation-root transform and a physical
ground query whose correct reference differs from the actor's starting height.
