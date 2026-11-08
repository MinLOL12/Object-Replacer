# Object Replacer

Swap any object in Geometry Dash for a selection of your own objects — in **every
level**, without editing it.

Made a custom spike out of 6 blocks and decorations? Select it, tell the mod
"every spike", and every single spike in the level becomes your custom spike —
in the level you are playing, in any online level, and in editor playtests.

It is not limited to spikes: saws, blocks, portals, decorations, whatever has an
object ID can be replaced.

## What it does

* Replaces **every** object with a given object ID, in the whole level at once.
* The replacement can be **one object or a whole selection** of objects; the
  selection keeps its shape, spacing and internal layering.
* The replacement **follows the original object**: position, rotation, scale,
  flips, groups, color channels and layers are carried over, so levels keep
  their look and their triggers keep working.
* Works on downloaded levels, your own levels and editor playtests, because the
  level is rewritten *before* the game creates it — the level file itself is
  never changed (unless you explicitly bake it in the editor).

## How to use it

1. Open a level in the **editor**.
2. Select the objects you want to use as the replacement (one object or many).
   This is your "custom spike", "custom saw", whatever you built.
3. Click the **Replace** button in the editor's top bar (or press **F5**).
4. Press **New Rule**.
5. Press **Capture Selection** — the selected objects are stored as the
   replacement.
6. Enter the **Target object ID** of the object you want to replace
   (a spike is `8`, a saw is `88`, a normal block is `1`, ...), or select that
   object in the editor and press **From Selection**.
7. Press **Save**, then playtest the level.

From then on, every object with that ID is replaced everywhere, in every level,
until you delete the rule or turn the mod off.

## Finding the object ID

Select the object in the editor and press **From Selection** in the rule editor —
that fills in its ID for you. Alternatively, Geode's "Show Object Info" (or the
object's ID shown in the editor) tells you the ID.

## Settings

| Setting | Default | What it does |
| --- | --- | --- |
| Enabled | on | Master switch for every replacement. |
| Apply in Editor | off | Also replace objects when a level is loaded in the editor. Saving the level with this on bakes the replacements into the level. |
| Keep Target Groups | on | Puts replacements into the replaced object's groups, so group triggers and color triggers still hit them. |
| Keep Target Colors | on | Uses the replaced object's color channels on the replacement. |
| Keep Target Layer | on | Uses the replaced object's editor layer / z layer / z order. |
| Follow Target Rotation & Scale | on | Rotates, scales and flips the replacement to match the object it replaces. |
| Open Object Replacer | F5 | Keybind that opens the menu. |

## Baking a replacement into a level

Turn on **Apply in Editor**, reopen the level in the editor and save it. The
replacement objects are now real objects in that level, and can be exported or
uploaded like anything else (uploading a level that uses this counts as using a
mod while verifying — respect the level rules of wherever you plan to use it).

## Notes and limitations

* Rules are keyed by object ID, so every object with that ID is replaced —
  including objects you added yourself with that ID.
* Triggers are not recommended as targets; the mod is meant for gameplay
  objects and decoration.
* A replacement is applied when a level is loaded. If you change a rule while a
  level is already open, restart / re-enter the level to see the change.
* Very large replacements multiplied over very many objects will, of course,
  produce a lot of objects. Keep an eye on the object count of your level.

## Building

```bash
git clone https://github.com/MinLOL12/Object-Replacer
cd Object-Replacer
GEODE_SDK=/path/to/geode cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Requires the [Geode SDK](https://github.com/geode-sdk/geode) for GD 2.2081.

The level string rewriter is standalone; its tests can be run without the SDK:

```bash
cd tests
g++ -std=c++20 -O1 -I../src LevelStringTest.cpp ../src/LevelString.cpp -o test && ./test
```
