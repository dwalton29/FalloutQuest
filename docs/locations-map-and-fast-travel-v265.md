# Locations, map controls and fast travel (v265)

Discovery now uses original REFR map-marker data and a physical 3D distance,
replacing the occupied-cell rectangle. The supplied master contains 206 markers.
Names are FULL, icon types are TNAM (the original 0–14 enumeration), and FNAM
separately authors visibility, travel availability and the Show All Hidden bit.
Live reference/enable-parent state controls whether a marker participates.

## Recovered source values and UI

The supplied Fallout3.exe Setting objects provide these defaults; GMST overrides
from Fallout3.esm are decoded separately without changing legacy catalog hashes.

| Setting | Executable default | Supplied master override |
|---|---:|---:|
| iMapMarkerRevealDistance | 1000 game units | absent |
| iXPRewardDiscoverMapMarker | 10 XP | 10 XP |
| fWorldMapMinZoom / MaxZoom | 0.75 / 5 | absent |
| fLocalMapMinZoom / MaxZoom | 0.1 / 0.9 | absent |
| fMoveBaseSpeed | 85 game units/second | 77 |
| fHostileActorExteriorDistance | 3000 game units | absent |
| fHostileActorInteriorDistance | 2000 game units | absent |

The discovery routine at executable VA 00934B11 constructs sDiscoveredText,
a space and the marker's authored name. At 00934B90 it assigns the original
`Interface\Icons\Message Icons\glow_message_map.dds`, with a 2-second message
hold. Its subsequent reward reads iXPRewardDiscoverMapMarker at 00934CBB.
The existing original-font HUD message and XP meter present that event. The
original UIPopUpExperienceUp / UILevelUp audio records resolve through the
installed Sound.bsa, preserving the XP system's perk-adjusted reward and audio.
No replacement sound or generated icon is embedded. The HUD icon resolver also
supports InterfaceShared.tai sprites and reuses its existing atlas when possible.

The world map continues using the original WRLD map image/MNAM/ONAM transforms,
with original icon assets. Marker selection is calculated before controller
input, in screen pixels, so zooming does not enlarge the selection radius in
world coordinates. Selected markers use the authored 1.75 size multiplier.
Names render with their font texture before any subsequent icon draw.

## State and scripts

Physical discovery awards XP once and queues the discovery notice. Authored
initial travel flags, script-granted travel and developer unlocks are already
known; they do not grant exploration XP when subsequently approached.

Visible-only and travel-enabled script reveals are distinct from actual
exploration. ShowMap's optional travel argument is honored. EnableFastTravel
updates a saved canonical flag and participates in normal script transaction
publication/rollback. A new optional MAP1 Pip-Boy save tail stores reveals,
travel grants, that flag and the last exterior map context. Older saves retain
their known locations; no historical XP or notifications are replayed.

For an interior, the map follows its authored XTEL door graph to an exterior
entrance. It falls back to saved exterior context when no exterior connection is
available. Small exterior worlds use their actual world identity, including when
the Wasteland rolling streamer is inactive.

## Controls and developer option

- DATA > Local Map or World Map: press A to enter map controls.
- Right stick pans. Hold the right trigger and move the stick vertically to zoom.
- A over a known world marker opens travel confirmation; A confirms, B cancels.
- A over empty world-map space still places/removes a waypoint.
- Pause > Debug > Unlock all map locations grants travel without XP or popups.
  Like the original ShowAllMapMarkers default, it skips Show All Hidden markers
  and disabled references, so obsolete/destroyed alternatives are not exposed.

World-map zoom uses the original bounds. Local-map geometry is normalized to its
captured extent, so native 0.1–0.9 magnification corresponds to a 1–9 zoom ratio.
Controller bindings, physical-screen placement and modal presentation are VR
adaptations of the existing authored UI assets.

## Fast-travel ownership and limits

Admission checks the selected marker again, the original CELL invert-travel and
WRLD no-travel flags, the script travel flag, health, encumbrance, current falling
state, combat targeting the player, and nearby hostile actors. The health-damage
gate observes damage in the current logical frame. Existing loading/resident
publication barriers exclude conflicting scene changes.

The original marker position/rotation and actual owning CELL/world feed the
existing loading and safe-arrival pipeline. Confirmation preserves the A latch
across loading. Travel hours are carried in the scene request and applied only
at successful publication, before the committed state is saved. Aborted loading
does not advance time.

The original long-distance path-length service is unavailable in this runtime.
Travel duration therefore uses direct distance in authored parent-map coordinates,
original walk speed and TimeScale. This is a standalone estimate; it does not
claim original navmesh route length. Fast travel does not implement follower
relocation or new encounter simulation.

## Verification

The production Player/menu tests cover 3D proximity, radius overrides, physical
world separation, initial/disabled markers, XP overrides, single-award behavior,
saved reveals/travel flags, script semantics, developer unlocks without XP,
travel restrictions, confirmation/cancellation/revalidation, zoom bounds, empty
map waypoints and interior XTEL mapping. The same loader was exercised against
the supplied master, including the Vault 101 cave's exterior connection.

Source integration guards cover loading/tracking admission, controller latching,
read-only marker rendering and arrival-only clock publication. Existing physics,
Pip-Boy rendering, player and detached script-transaction checks also pass locally.
Android/Vulkan CI supplies native compilation and the wider regression suite.
Headset interaction and original asset presentation still require device use.

Reproduce source metadata without extracting any assets into the repository:

```sh
python tools/map/audit_locations.py /path/Fallout3.esm /path/Fallout3.exe --output /tmp/locations.json
```
