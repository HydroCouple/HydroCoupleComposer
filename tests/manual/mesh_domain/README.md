# Manual test — the mesh domain (E1a–E1c), 2026-08-29

What this exercises is everything the Mesh tab can currently do: drawing a
domain, correcting it, and building one out of GIS features already on the
map. **Mesh generation has no UI yet** — `generateMesh()` exists and is
tested, but the dialog, the preview layer and the UGRID writer are E2b–E2d.
Nothing below asks you to click a button that is not there.

The data sits over the Jordan River in Salt Lake City so a basemap gives it
context. Coordinates are WGS84; the map works in Web Mercator and reprojects
on the way in, which is one of the things worth checking.

| File | Holds | There to exercise |
| --- | --- | --- |
| `catchments.geojson` | 4 polygon features | rings, holes, MultiPolygon, a degenerate ring |
| `channels.geojson` | 1 line + 1 multi-line | breaklines, multi-part features |
| `gauges.geojson` | 2 points + 1 multipoint | forced points, multi-part features |

Launch: `build/darwin/HydroCoupleComposer.app/Contents/MacOS/HydroCoupleComposer`

---

## 1. Get the data on the map

1. **Map** tab → **Add Vector** → open `catchments.geojson`.
2. Repeat for `channels.geojson` and `gauges.geojson`.
3. Optional: **View** menu → **Basemap** → pick a provider (needs network).

**Expect:** three layers in the Layers panel, drawn over Salt Lake City. The
view frames each layer as it is added.

**The point:** the files are in degrees and the map is in metres. If the
catchment lands somewhere plausible rather than a pixel from the origin, the
reprojection is working.

## 2. Build a domain out of the GIS features (E1c)

4. Select `catchments` in the **Layers** panel — the import reads the layer
   you are pointing at.
5. **Map** tab → **Select** tool. Drag a rubber band around all four
   catchment features. (A single click picks one feature; the band takes
   every feature it crosses.)
6. **Mesh** tab → **From Selection ▾** → **As Boundary**.

**Expect**, in the log panel at the bottom:

> Imported a boundary and 4 holes from "catchments". 1 of them enclosed no
> ground and were left out.

and on the map: the big catchment outlined as the boundary, with **four**
filled holes inside it — the lake, the north lobe, and *both* twin ponds.

**The point — read this before deciding it looks right:**

- The **lake** is an interior ring of the same feature as the boundary. It
  became a hole because it is *inside* another ring, not because it came
  second in the file.
- **Twin ponds is one feature holding two separate polygons.** Both are
  holes. The reader flattens a MultiPolygon's rings into one list, so the
  obvious rule — first ring is the outer one, the rest are its holes — would
  have cut the second pond out of the first and you would see **three**
  holes, not four. Count them.
- **Survey error** is three points on a diagonal. It has a perfectly good
  bounding box and no area, so it was skipped and the message says so. If
  the log does not mention it, geometry went missing quietly.

7. Now select `channels` in the Layers panel, band-select both features, and
   **From Selection ▾** → **As Breaklines**.

**Expect:** *three* breaklines from two features — the main channel plus the
two the "Tributaries" multi-line carries.

8. Same again with `gauges` → **As Forced Points**.

**Expect:** *four* points from three features.

## 3. Refusals should explain themselves

9. With `channels` still current, try **From Selection ▾** → **As Boundary**.

**Expect:** a refusal naming both halves — the layer holds lines, and a
boundary is made of polygons. Not "cannot import".

10. Click empty map to clear the selection, then try any import.

**Expect:** a refusal naming the layer, and nothing added.

## 4. Draw a domain by hand (E1b-2)

11. **Mesh** tab → **Boundary**. Click three or more corners, then
    **right-click** to finish.

**Expect:** the ring appears as you click, the segment to the cursor follows
the pointer without being stored, and the right-click finishes without
leaving a corner where you clicked it.

12. Draw a **Hole** inside it, a **Breakline**, and a **Point** (a point
    finishes on its own first click).
13. Start another boundary and, mid-shape, switch to another tool.

**Expect:** the half-drawn shape is discarded, not committed. Three clicks is
enough to make a ring, which is exactly why switching must not finish one.

## 5. Correct it (E1b-3)

14. **Mesh** tab → **Edit**. Square handles appear on every domain vertex.
15. Hover a vertex — it highlights. Drag it — it moves as you drag.
16. Drag a vertex to within ~10 px of another and release.

**Expect:** it lands *exactly* on the other one. A gap of half a pixel is
invisible and is a hole the triangulator would mesh straight through.

17. Drag a vertex a very short distance — two or three pixels.

**Expect:** it moves. If it snapped back to where it started, the dragged
vertex is being treated as its own neighbour.

18. Click on an **edge** between two vertices and keep dragging.

**Expect:** one gesture — a corner is inserted where you clicked and the same
drag moves it.

19. **Right-click** a vertex.

**Expect:** it is removed. Take a hole down to two corners and the whole hole
goes rather than staying as something that is no longer a hole.

20. Start a drag and switch tools mid-drag.

**Expect:** the vertex goes back where it started, and the handles disappear
with the tool.

## 6. Persistence (E1a)

21. **File** → **Save As…**, save the composition, then reopen it.

**Expect:** the domain comes back — boundary, holes, breaklines and points.
It rides in the presentation sidecar beside the composition.

---

## What to write down

For anything that misbehaves, note **which step**, what you expected, what
happened, and whether the log panel said anything. The counts in steps 6–8
(4 holes, 3 breaklines, 4 points) are the ones most likely to be quietly
wrong, since a plausible number is exactly what a broken rule produces.
