# Art standards

The rules Manifest's models, textures and materials are authored against: where files live, what
they are called, the scale, axes and geometry, the materials and textures, and the texture
coordinate sets. Manifest defines them; [Jettison](../../jettison/docs/README.md) supports the
workflow (its cook, formats and renderer) and its own test content follows the same rules, so one
standard serves both. Budgets (triangle counts, texture sizes, texel densities) are the artists'
to set, per asset class, and are open below.

Settled with the user on 2026-11-27 ([D-118](decision-ledger.md#d-118)). A rule marked
**(engine)** waits on Jettison work before the cook or renderer honours it; the items are in [what
Jettison supports](#what-jettison-supports).

## What the rules serve

Jettison draws with clustered forward shading and glTF's metallic-roughness model. Models are cooked
into a meshlet hierarchy, with levels of detail made by the cook, and drawn by mesh shaders where
the device has them. Shadows, ambient occlusion and sharp reflections are ray traced where the
device has ray queries, with cascades, screen-space occlusion and the environment cube otherwise
([dense geometry and ray tracing](../../jettison/docs/dense-geometry-and-ray-tracing.md)). That
changes what an artist should do, against habits from older engines:

- **Model the form.** Dense geometry is cheap and ray-traced shadows and occlusion see only
  geometry: a bevel, panel line or rivet in the mesh shades and shadows itself; one in a normal map
  does neither. Normal maps carry detail finer than the mesh, not the shape.
- **No hand-made levels of detail**, impostors or shadow proxies. The cook simplifies, and ray
  tracing uses a level the cook picks.
- **Nothing is faked for one view.** Reflections and shadows show the back and underside of what
  the camera cannot see, so a hero object is whole.
- **No light baked into textures.** The renderer lights everything; base colour is the surface's
  colour under white light.

## Concepts

| Term | Meaning |
| --- | --- |
| Work tree | The artists' working files (`.blend`, Substance projects, sculpts, references). Never cooked or shipped. |
| Source tree | Exports only: glTF, PNG, material files and cook settings. The cook's input. |
| Content root | Where cooked files land: `mods/manifest/` in Manifest's repository, packed for release ([files, packs and mods](../../jettison/docs/files-and-mods.md)). |
| Asset | One named thing with a folder of its own: a ship, a station module, a prop, a character. |
| Asset class | A kind of asset with shared rules and budgets: `ships`, `stations`, `kits`, `props`, `characters`, `planets`, `effects`. A folder under `models/` and `textures/`. |
| Material library | The material files under `materials/`. A model names its materials; the library says how each looks. |
| UV set | One of a mesh's texture-coordinate sets (glTF's `TEXCOORD_n`), with a role name declared per asset. |
| Trim sheet | A tiling texture of strips (edges, panels, vents) many assets map onto through one UV set. |

## Files

### The trees

```text
manifest-art/                            the art repository, outside Manifest's
  work/                                  working files: never cooked
    models/ships/hauler-a/hauler-a.blend
    textures/ships/hauler-a/hauler-a.spp
  source/                                exports only: the cook's source root
    cook.yaml                            Manifest's cook settings
    models/ships/hauler-a/
      hauler-a.gltf                      geometry, rig, clips, material names
      hauler-a.bin
      hauler-a.gltf.yaml                 this asset's cook settings: its UV sets, overrides
    textures/ships/hauler-a/
      hauler-a-color.png
      hauler-a-normal.png
      hauler-a-orm.png
    textures/trims/hull-plating-a/
      hull-plating-a-color.png ...
    materials/ships/
      hauler-a-hull.material.yaml
      hauler-a-glass.material.yaml
    materials/trims/
      hull-plating-a.material.yaml
```

- **Work and source are separate trees**, because the cook copies through any file it has no
  processor for: a `.blend` in the source tree would ship.
- **The cooked tree mirrors the source tree** under the content root, with cooked extensions
  (`models/ships/hauler-a/hauler-a.glb`, `.meshlets`; `textures/.../*.ktx2`). Run once, or
  `jettison_cook --watch` while working, and a running game reloads what changes.
- **Cooked files are not edited by hand.** The source tree is the authority; a re-cook overwrites.
- **The art repository is Git with Git LFS** ([D-119](decision-ledger.md#d-119)): binary files
  (`.blend`, `.spp`, `.png`, `.bin`) are LFS objects, and text (`.gltf`, `.yaml`) stays plain
  Git, so a change to a setting or a material diffs.

### Names

Every file and folder name, and every name inside a glTF file, is kebab-case: lower-case ASCII
letters and digits joined by single hyphens. The cook refuses a file that breaks the rule, naming it
([content files](../../jettison/docs/coding-standards.md#content-files)).

- **An asset** is `<subject>[-<descriptor>...][-<variant>]`, most general word first, so related
  assets sort together: `hauler-a`, `cargo-pod-small-b`, `station-ring-hub-a`. A variant is a
  letter. No `final`, `new`, `v2`, initials or dates: version control keeps history.
- **A kit piece** adds its size in whole metres: `station-kit-corridor-straight-4m`,
  `station-kit-corridor-corner-4m`.
- **A texture** is `textures/<class>/<asset>/<asset>-<map>.png`. The asset's name is repeated so a
  file is still known out of its folder. The map words:

| Map | Holds | Colour space | Source |
| --- | --- | --- | --- |
| `color` | Base colour; alpha is opacity where the material is masked or blended | sRGB | 8-bit PNG |
| `normal` | Tangent-space normal, +Y up (OpenGL, as glTF) | Linear | 16-bit PNG |
| `orm` | R occlusion, G roughness, B metallic: glTF's packing | Linear | 8-bit PNG |
| `emissive` | Emitted colour; its intensity is the material's | sRGB | 8-bit PNG |
| `height` | Displacement, for programs that read it | Linear | 16-bit PNG |
| `mask` | Up to four masks (livery, wear, dirt), their meaning the material's | Linear | 8-bit PNG |
| `roughness`, `metallic` | Single channels, for programs that take them alone (`planet`) | Linear | 8-bit PNG |

  `detail-` before a map word marks a detail layer (`hull-plating-a-detail-normal.png`); a
  second mask is `mask-b`. The map word sets the colour space when the cook encodes, so no file
  carries its colour space in its name **(engine)**.
- **A material** is `materials/<class>/<name>.material.yaml`: an asset's own as
  `<asset>-<part>` (`hauler-a-hull`), a shared one by what it is (`hull-plating-a`).
- **Inside a glTF:** nodes, meshes and clips are kebab-case (`landing-gear-front`; clips
  `idle`, `walk-forward`, `door-open`). Joints too, sides as a suffix, which Blender's mirror
  tools read: `upper-arm-l`, `upper-arm-r`. Each glTF material's name is the name of the library
  material it draws with. Faces' blend shapes are the one exception: ARKit's 52 names exactly
  (`jawOpen`, `eyeBlinkLeft`), as the [characters plan](../../jettison/docs/characters.md) has
  them.
- **Nodes with a role** carry it as a prefix **(engine)**: `collision-<name>` for a collision
  mesh ([J12](../../jettison/docs/history.md#j12)), never drawn; `socket-<name>` for an empty that
  marks where something attaches. Lights and cameras are placed in scene files, not exported.

## Scale, axes and origins

- **One unit is one metre.** Blender's scene is metric at unit scale 1.0. Transforms are applied
  before export, so every object has scale 1 and no rotation of its own.
- **+Y up and right-handed**, as glTF and Jettison are ([coordinate
  frames](../../jettison/docs/coordinates.md)). Blender's exporter makes Blender's +Z glTF's +Y.
- **A vehicle's nose points along -Z, starboard +X, top +Y** in the file, the engine's vehicle axes
  ([docks](../../jettison/docs/docks.md)). In Blender that is the nose along +Y. Characters and
  props face -Z too, so one forward serves everything.
- **Origins:** a prop's at the centre of its base; a vehicle's at the centre of its bounds; a kit
  piece's on its snapping grid, at the centre of its floor.

## Geometry

- **Triangulate before baking**, and export the triangulated mesh, so the cook's MikkTSpace
  tangents agree with the bake's. Do not export tangents.
- **Clean meshes:** no non-manifold edges, zero-area faces, loose vertices or interior faces;
  vertices merged by distance.
- **Even triangles.** Long thin triangles and very large ones bound meshlets poorly and simplify
  badly. Dense regions are fine.
- **Closed where it casts a shadow.** Rays see back faces: a single-sided plane shadows from both
  sides, and a gap at a join leaks light. Kit pieces overlap a little at their joins rather than meet
  edge to edge. A thin surface meant to be seen from both sides is marked double-sided.
- **Split only where needed.** Each UV seam and hard edge duplicates vertices and holds the
  simplifier back along it.
- **Normals:** custom normals exported; weighted normals for hard surfaces.
- **Skinning:** at most eight joints a vertex, weights normalised.
- **Blend shapes** are exported with their normals.
- **Vertex colours** are linear data with a meaning the material gives (a mask), not lighting.
- **No alpha-tested aggregates** (foliage, chain-link fences): hair cards are the only alpha-tested
  geometry ([J59](../../jettison/docs/characters.md#j59-hair-cards-and-alpha-tested-materials)).

## Materials

- **Metallic-roughness only**, glTF's core model. No specular-glossiness; no other glTF material
  extension (clearcoat, sheen, transmission) until the engine supports it, and the cook refuses one
  **(engine)**.
- **The library holds the look.** A glTF carries geometry, rig, clips and material names; the
  material file of that name holds the program, values and textures, so a shared material is
  changed once **(engine)**. Blender's exporter writes no images (Images: None); textures go straight
  from Substance or Blender into the texture tree.
- **Values:**
  - base colour has no lighting, shadow or occlusion in it. A non-metal sits between about 30 and
    240 sRGB (coal to snow); a metal's is its measured reflectance, about 180 to 255;
  - metallic is 0 or 1. Values between are for texels that mix the two (dirt over steel);
  - roughness tells the surface's story: wear, grime and polish;
  - occlusion holds only the cavities the geometry cannot show, a few centimetres and smaller; ray
    tracing and screen-space occlusion supply the rest;
  - emissive intensity is in the HDR target's linear units ([materials and
    programs](../../jettison/docs/rendering.md#materials-and-programs)). An emissive surface lights
    nothing else (there is no global illumination): a light placed in the scene does.
- **Alpha:** opaque by default; masked only for hair cards; blended only through the `glass`
  program ([transparency](../../jettison/docs/rendering-effects.md#transparency)).
- **Fewer materials a mesh.** Each material on a mesh is a draw of its own: trim sheets and masks
  serve better than many small materials.

## Textures

- **Sources are PNG**: 8-bit, 16-bit for normal and height maps. No JPEG: the cook compresses, and a
  lossy source is compressed twice.
- **Sizes are powers of two** on each side, not necessarily square, so block compression and mip
  chains hold. The cook makes the mips and encodes KTX 2 (UASTC for colour and normal maps).
- **Dilate** past every UV island's edge, to fill, so mips do not bleed the background in.
- **Normal maps are OpenGL's** (+Y up, green up), as glTF; Substance's OpenGL preset. The cook cannot
  tell the conventions apart, so a DirectX map shows as wrong lighting, not an error.
- **Texel density** is stated in pixels per metre, per asset class; its values are open.

## Texture coordinate sets

### What glTF gives

A glTF mesh may carry any number of sets, `TEXCOORD_0` up, and each texture a material reads names
the set it reads (`texCoord`). The sets have no names: Blender exports its UV maps in their order
in the list and drops their names.

### The rules

- **Set 0, `unique`, is always present:** every face, no overlaps, inside 0 to 1. Bakes (normal,
  occlusion), masks and an asset's own textures read it.
- **Further sets have roles**, declared in the asset's cook settings in the order Blender lists them:

```yaml
# models/ships/hauler-a/hauler-a.gltf.yaml
uvSets: [unique, trim, detail, decal]
```

- **A material slot reads a set by role**, so a library material serves assets whose sets differ in
  number and order **(engine)**:

```yaml
# materials/trims/hull-plating-a.material.yaml
textures:
  - slot: baseColor
    path: textures/trims/hull-plating-a/hull-plating-a-color.png
    uv: trim
  - slot: detailNormal
    path: textures/trims/hull-plating-a/hull-plating-a-detail-normal.png
    uv: detail
```

- **The roles:**

| Role | For | Layout |
| --- | --- | --- |
| `unique` | Bakes, masks, an asset's own textures | No overlaps, inside 0 to 1 |
| `trim` | A trim sheet | Strips on the sheet's rows; overlaps expected |
| `tile` | A tiling material | Any range, at the class's texel density |
| `detail` | A detail layer over another set | Any range |
| `decal` | Markings, numbers, livery from an atlas | Only the faces that carry one; the rest collapsed to a corner |
| `atlas` | A shared atlas of small parts (screens, labels) | Inside 0 to 1 |

  These are the defaults for typical assets. Which asset classes need six sets is not yet known;
  a class's usual `uvSets` is recorded here when one settles. A new role is added to this table
  first. A mesh with more than one set and no `uvSets` is
  refused, as is a material that reads a role the mesh does not declare, each error naming the files
  **(engine)**.
- **At most eight sets a mesh**: a material has at most eight texture slots, so a ninth set could
  never be read.
- **Each set costs every vertex**, about 4 bytes once quantised, more on a dense mesh: declare only
  the sets an asset's materials read.

### How Jettison draws them **(engine)**

The shaders fetch each set from a stream of its own, by vertex pulling, and only the sets a
material reads; the cook quantises each set over its own range. The design, and why a fixed vertex
or a shader variant per count was rejected, are Jettison's
[J64](../../jettison/docs/roadmap.md#j64). Vertex colour sets (`COLOR_n`) go the same way, declared
as `colorSets` with roles.

## Exporting from Blender

The glTF 2.0 exporter, format glTF Separate (`.gltf` + `.bin`):

- **Include:** selected objects or the asset's collection; custom properties off.
- **Transform:** +Y up on.
- **Data, mesh:** apply modifiers; UVs; normals; tangents off; vertex colours only when a material
  reads them; materials exported, images none.
- **Compression:** off. The cook compresses.
- **Animation:** clips as actions, named as above; shape keys with their normals for blend shapes.

The source tree's `cook.yaml` and each asset's settings file are described in Jettison's
[cook](../../jettison/docs/dense-geometry-and-ray-tracing.md#the-cook).

## Budgets: open

The artists set these per asset class, and this document records them when set: triangles at the
finest level; texture sizes per map; texel density; UV sets; materials a mesh; joints and blend
shapes for characters.

## What Jettison supports

Jettison's side of the standard is a track of its roadmap, [the art standards'
support](../../jettison/docs/roadmap.md#a-separate-track-the-art-standards-support):

- [J64](../../jettison/docs/roadmap.md#j64): texture coordinate sets by role, through vertex
  pulling, at most eight a mesh, and vertex colour sets the same way.
- [J65](../../jettison/docs/roadmap.md#j65): the material library, bound by a glTF material's name.
- [J66](../../jettison/docs/roadmap.md#j66): the cook reads the map words (colour space, `orm`,
  `mask`, 16-bit sources), refuses material extensions, and reads `collision-` and `socket-`
  nodes, the collision mesh with [J12](../../jettison/docs/history.md#j12).
- [J67](../../jettison/docs/roadmap.md#j67): Jettison's test content laid out by this standard, and
  its content-file rule pointing here.
