# Atmospheric shading improvement plan

The atmosphere scene implements Sébastien Hillaire's *A Scalable and Production
Ready Sky and Atmosphere Rendering Technique* (EGSR 2020), the technique that
ships as Unreal's `SkyAtmosphere` component. Instead of Bruneton and Neyret's
precomputed 4D scattering table, it rebuilds a handful of small LUTs every
frame, so the medium parameters and the time of day can animate freely.

Code lives in:

| Area | Path |
| --- | --- |
| Pass graph and parameters | `Crisp/Crisp/Models/Atmosphere.{hpp,cpp}` |
| Scene and tuning ui | `Crisp/Crisp/Scenes/AtmosphereScene.{hpp,cpp}` |
| Shared shader code | `Crisp/Crisp/Shaders/Common/atmosphere.part.glsl` |
| Passes | `Crisp/Crisp/Shaders/sky-*.{frag,comp,vert,geom}.glsl` |

## Current state

| Pass | Output | Size | Consumed by |
| --- | --- | --- | --- |
| `transmittanceLutPass` | transmittance | 256x64 | every other pass |
| `multipleScatteringPass` | `Psi_ms` | 32x32 | sky view, camera volumes, ray march |
| `skyViewLutPass` | distant sky | 192x108 | ray march, on the fast sky path |
| `viewVolumePass` | aerial perspective froxels | 32x32x32 | ray march, once a depth source exists |
| `rayMarchingPass` | scene radiance | swap chain | tonemap |
| `tonemapPass` | display referred, linear | swap chain | present |

Every size in that table comes from the `k*Lut*` constants in
`Models/Atmosphere.hpp`, mirrored in `Common/atmosphere.part.glsl`.

Open sky now resolves through the sky view LUT, so the distant-sky half of the
technique pays for itself. The aerial perspective volume is still built every
frame and discarded: its consumer is written and correct, but nothing in the
scene rasterizes geometry for it to apply to — see item 5.

## Units: keep kilometres, convert at the boundary [Done]

Every length in the uniform block is in kilometres and every coefficient is in
inverse kilometres, matching Hillaire, Bruneton, and Unreal. The scene works in
metres, and `kMetersPerKilometer` converts between them at every crossing.

Switching the internals to metres instead would have been numerically neutral —
IEEE 754 relative precision does not depend on the exponent, so the cancellation
in `raySphereIntersectNearest` (`dot(o, o) - r * r`, two terms that agree to five
digits) is exactly as lossy either way. What that switch would have cost is a
sweep of every magic constant carrying an implied unit — `kPlanetRadiusOffset`,
`kCameraVolumeKmPerSlice`, the `clamp(tMax * 0.01, 0, 1)` sample-count ramp, the
`9000000` distance sentinel, the ozone tent terms, the ui drag speeds and format
strings — where missing one is a silent, plausible-looking bug. It also puts
every published coefficient one transcription away from wrong.

The problem worth solving is the *interface*, not the internals, so the
conversion sits where scene units meet atmosphere units and nowhere else.
`kMetersPerKilometer` is mirrored in `Models/Atmosphere.hpp` and
`Common/atmosphere.part.glsl` alongside the LUT constants, and is applied at:

- `cameraPosition` on upload in `AtmosphereScene::update`, the only field of the
  uniform block that comes from the scene rather than being authored in km
- all five positions reconstructed from `invVP` — the primary ray direction in
  `sky-ray-march.frag` and `sky-camera-volumes.frag`, the froxel `tDepth` in the
  aerial perspective path, and the depth clamp inside both copies of
  `integrateScatteredRadiance`

That last group was a real bug, not just a naming problem. `invVP` yields scene
space, and the results were compared against `tMax` and subtracted from
`cameraPosition`, both in kilometres. The direction reconstructions were
`normalize()`d, so a uniform scale would have cancelled — but the subtraction
happens *before* the normalize, so mixing metres and kilometres there skews the
ray rather than cancelling. It was inert only because the scene declared one
world unit to be one kilometre.

The scene side follows: the camera starts at `kInitialCameraPosition`
(1 km up, in metres), its speed slider is m/s, and the ui prints position in
metres with altitude in km, since altitude reads against `skyViewLutMaxAltitude`
and the planet radii.

One named constant keeps the sensitive geometry in the units its reference
material is published in, and makes the atmosphere reusable from a scene in any
unit by changing a single value.

### Radiometric, not photometric [Done]

The reference implementation names its integral `IntegrateScatteredLuminance`
and its sun term `Illuminance`, which is the photometric pairing Unreal uses:
lux in, cd/m^2 out. That pairing is dimensionally correct — the phase function
contributes 1/sr and the division by extinction integrates over path length, so
whatever the sun term is, the march returns it per steradian.

This port now uses the radiometric spelling instead: `sunIrradiance`,
`integrateScatteredRadiance`, `getSunRadiance`, `sunDiskRadiance`. The integral
is agnostic between the two and so are the medium coefficients, since scattering
in 1/km is neutral; the sun term is the only thing that picks a convention.
Radiometric was chosen because nothing here applies a luminous efficiency
function, so calling the result a luminance claimed a weighting that does not
exist.

Note that the values are not physical either way. `sunIrradiance` defaults to 1
against a real solar constant of about 1361 W/m^2, `exposure` is a bare
multiplier, and `sunDiskRadiance` is authored against the sky it sits in. Going
physical means scaling the sun term spectrally and giving `exposure` a camera
model, which is the same work item 7 wants — see the comment on `sunIrradiance`
in `Models/Atmosphere.hpp`.

## Tier 1 — correctness [Done]

These changed what the renderer produces, so they landed before anything was
tuned against the old look.

### 1. Use the isotropic phase in the multiple scattering LUT [Done]

`sky-multiple-scattering.comp.glsl` accepts a `MieRayPhase` argument, is called
with `false`, and then ignores it — it applies the directional Mie and Rayleigh
phases anyway, and multiplies by `medium.scattering` a second time on top of the
`phaseTimesScattering` term that already contains it:

```glsl
// current
const vec3 phaseTimesScattering = medium.scatteringMie * miePhaseValue + medium.scatteringRay * rayleighPhaseValue;
const vec3 S = globalL * (earthShadow * shadow * transmittanceToSun * phaseTimesScattering * medium.scattering);

// intended
const vec3 phaseTimesScattering = MieRayPhase
    ? medium.scatteringMie * miePhaseValue + medium.scatteringRay * rayleighPhaseValue
    : medium.scattering * isotropicPhaseValue;
const vec3 S = globalL * (earthShadow * shadow * transmittanceToSun * phaseTimesScattering);
```

Assuming scattering beyond the first order is isotropic is the entire basis for
collapsing orders 2..inf into the geometric series `Psi_ms = L_2 / (1 - f_ms)`,
so a directional phase here is not an approximation, it is a different quantity.
The extra `sigma_s` factor leaves the term roughly thirty times too dark, which
is most of why the sky reads flat and needs a large exposure multiplier.

Also fixed in the same file: `f_ms` is clamped below 1 before the reciprocal, the
parallel reduction indexes with `gl_LocalInvocationID.z` rather than
`gl_GlobalInvocationID.z` (equal only because the dispatch is one group deep),
and the inner `uint i` no longer shadows the outer sample index.

### 2. Adopt the engine's reverse-Z convention [Done]

`Camera.cpp` builds an infinite reverse-Z projection — near maps to 1, infinity
to 0. The ported shaders carried Unreal's convention: `sky-ray-march.frag`
treated `fragmentDepth == 1.0f` as "no geometry" and clamped the march only when
`ClipSpace.z < 1.0f`. Both comparisons inverted. `kFarPlaneDepth` and
`kNoDepthBuffer` now live in `Common/atmosphere.part.glsl` and name the
convention, since `kFarPlaneDepth` is 0 and can no longer double as the "no depth
buffer supplied" sentinel the way `-1` used to.

The depth path is still dead — nothing writes `ViewDepthTexture` — so this was
free to fix now and would have been expensive to discover later.

### 3. Finish the Z-up to Y-up port [Done]

Two leftovers from the Unreal source assumed Z is up:

- `sky-ray-march.frag` and `sky-camera-volumes.frag` undid the planet offset with
  `WorldPos + vec3(0.0, 0.0, -bottomRadius)`; this port offsets along Y.
- `sky-camera-volumes.frag` built its ray direction from
  `uv * vec2(2, -2) - vec2(1, -1)`, but the projection matrix already carries the
  Vulkan Y flip, so the froxel rays were mirrored about the horizon. The final
  ray march gets this right and documents why. The same expression in
  `sky-view-lut.frag` was harmless because `worldDir` is recomputed from the LUT
  parameterization a few lines later — that dead reconstruction is now deleted
  rather than left as a trap.

Verified by pinning the sun to 0 degrees elevation: the disk lands exactly on
the horizon, so the vertical mapping is neither mirrored nor offset.

## Tier 2 — actually use the LUTs [Partial]

### 4. Sample the sky view LUT for distant sky [Done]

`sky-ray-march.frag` now takes the LUT path when a pixel has no geometry and the
camera is below `skyViewLutMaxAltitude` (4 km by default), resolving open sky
with one texture fetch instead of a march. Above that altitude, or with
`fastSkyEnabled` off, it falls back to the full march.

`skyViewLutParamsToUv` is the new inverse of `uvToSkyViewLutParams`. Both now
live in `Common/atmosphere.part.glsl` rather than one in the producer and one in
the consumer, because the horizon-aligned quadratic spread has to match exactly
on both sides or the horizon shears.

Two degeneracies the reference glosses over are guarded: `cross(up, viewDir)`
collapses when looking straight up or down, and the sun's azimuth is undefined
when it sits at the zenith, which the elevation slider can reach.

Verified by capturing the same view with `fastSkyEnabled` on and off and
differencing the sky region: mean absolute error 0.2 of 765 summed over RGB,
with 0.019 percent of pixels above 8. That is sRGB quantization noise, i.e. the
LUT path and the reference march agree.

### 5. Sample the aerial perspective volume for geometry [Partial — blocked on a depth source]

The consumer is now real code rather than a commented block. `sky-ray-march.frag`
gains `sampleAerialPerspective`, gated on a new `fastAerialPerspectiveEnabled`
uniform that mirrors `fastSkyEnabled`, and takes the froxel path for any shaded
pixel closer than `kCameraVolumeMaxDistance` (128 km, the full reach of the
volume). Everything past that falls through to the march, as does every pixel
when the toggle is off.

Two things were wrong in the commented version and are fixed:

- It fed the normalized depth `w` in [0,1] straight into `textureLod` on
  `cameraVolumeLut`. That is a `sampler2DArray`, not the 3D texture the reference
  uses, so the third coordinate is a layer index — `w` would have selected layer 0
  for every pixel in the volume. The slice mapping is now inverted properly to
  `layer = sqrt(N * t / kmPerSlice) - 0.5`, and since an array sampler cannot
  filter across slices, the two neighbouring layers are blended by hand.
- It reconstructed the camera as `worldPos + vec3(0, -bottomRadius, 0)`, which is
  just `cameraPosition` with the planet offset undone and back again. It now
  subtracts `cameraPosition` directly, which also makes the scene-unit-versus-km
  question visible in one place rather than buried — see the Units section.

What remains is the depth source. Nothing rasterizes in this scene, so nothing
writes `viewDepthTexture` (set 1, binding 4, still deliberately unwritten in
`Atmosphere.cpp`), and `fragmentDepth` is pinned to `kFarPlaneDepth` at a single
clearly marked line in `main`. Every pixel therefore reads as open sky and the
froxel path is unreachable — the compiler will fold it out entirely. The ground
from item 6 does not help, since it is found by sphere intersection inside the
march rather than rasterized.

Turning this on is now two changes, not a port: write a depth pre-pass into
binding 4, and swap that one line back to the texture fetch it already carries as
a comment. Note that until the descriptor is actually written, sampling it would
be a Vulkan validation error, which is why the seam is at the fetch rather than
behind the uniform toggle.

### 6. Render the ground [Done]

The ground bounce term is live in `sky-ray-march.frag` and `sky-view-lut.frag`,
gated on `renderGround`, and `groundAlbedo` now defaults to 0.3 rather than 0.
The surface is Lambert, lit by the sun through the transmittance LUT and seen
through the accumulated throughput of the medium in front of it.

Both shaders had to get the term, not just the final march: once the fast sky
path is on, everything below the horizon comes from the LUT, so a ground that
existed only in the march would appear and disappear with `fastSkyEnabled`.

Note this is a deliberate departure from the reference implementation, which
passes `ground = false` in both places and leaves the planet black behind the
haze. Only its multiple scattering LUT bounces light off the ground.

## Tier 3 — presentation [Partial]

### 7. Tone mapping [Done]

`tonemapPass` sits between the ray march and the present, reading
`rayMarchedImage` and writing `tonemappedImage`. The chain is now: ray march
writes scene radiance, the tonemap pass applies exposure and a curve and outputs
**linear** display referred values, and `GammaCorrect.frag.slang` stays the last
step with the sRGB encode. The intermediate is `R16G16B16A16_SFLOAT` rather than
8 bit, so the curve output is not quantized before the encode gets a chance to
distribute the error.

Four operators, selectable at runtime from `kTonemapOperatorNames`: none (clamp,
which reproduces what the scene did before), extended Reinhard with a white
point, Stephen Hill's ACES RRT+ODT fit, and Uchimura's GT curve with its toe,
linear section and shoulder exposed. The curves live in
`Shaders/Common/tonemap.part.glsl`, deliberately apart from the pass itself.

`exposure` moved out of `AtmosphereParameters` into `TonemapParameters` and is
gone from the atmosphere uniform block entirely — the ray march no longer scales
anything on its way out. Two consequences worth knowing:

- The debug LUT views are no longer pre-multiplied by exposure. They were the
  only consumers besides the final image, and they now go out raw like
  everything else, with the tonemapper making them visible. To read a LUT's
  actual values, set the operator to none and exposure to 1.
- Removing the field shifted every std140 offset after it. Re-verified against
  `glslangValidator -q`: the tail now runs 304 through 332, matching
  `AtmosphereParameters`.

Item 12's dirty flagging would extend naturally to this pass, which currently
re-runs every frame over the whole swap chain for what is a per-pixel function
of one texture.

### 8. Sun disk [Done]

`getSunRadiance` was a hard threshold against the disc half-angle. It now fades
across roughly one pixel, using `fwidth` of the angle to the sun. That derivative
is taken in `main` before any branch that can diverge across a quad, since screen
space derivatives are only defined in uniform control flow — worth keeping in
mind when the depth pre-pass eventually makes `fragmentDepth` non-uniform.

Treating the fade as a coverage fraction also dims a sun smaller than a pixel,
which is the physically right answer rather than a special case.

The disc is no longer flat. `computeSunLimbDarkening` applies the Hestroffer and
Magnan power law, cited in full in the shader — see the References section below.

Still open: the disc is not attenuated by the atmosphere along the view ray, so
it stays white while the sky around it reddens at sunset. Multiplying by the
transmittance to the top of the atmosphere would fix that.

## Tier 4 — code health [Partial]

### 9. Hoist the duplicated shader code [Partial]

Hoisted into `Common/atmosphere.part.glsl`: `sampleTransmittanceLut`,
`sampleMultipleScattering`, `fromUnitToSubUvs`, `fromSubUvsToUnit`, `getShadow`,
`sampleMedium`, `raySphereIntersectNearest`, `intersectAtmosphere`,
`moveToTopAtmosphere`, both transmittance and sky view LUT parameterizations, and
the phase functions. The duplicate `hgPhase`, `CornetteShanksMiePhaseFunction`,
and `LutTransmittanceParamsToUv` in `sky-camera-volumes.frag` are gone.

What remains is the item's headline: `SingleScatteringResult` and
`integrateScatteredRadiance` are still copy-pasted across four shaders —
`sky-ray-march.frag`, `sky-view-lut.frag`, `sky-multiple-scattering.comp`, and
`sky-camera-volumes.frag` — and have drifted on eleven separate axes. See the
consolidation analysis for the full list; the load-bearing differences are the
return payload, the variable sample count branch, the phase selection, the
multiple scattering feedback term, and the ground bounce guard. The blocking
constraint is that `sky-multiple-scattering.comp` is the pass that *produces* the
multiple scattering LUT and therefore has no sampler for it to bind.

### 10. One source of truth for LUT dimensions [Done]

`kTransmittanceLutWidth`, `kTransmittanceLutHeight`,
`kMultiScatteringLutResolution`, `kSkyViewLutWidth`, `kSkyViewLutHeight`,
`kCameraVolumeLut{Width,Height,SliceCount}` and `kCameraVolumeKmPerSlice` now
live in `Models/Atmosphere.hpp`, drive every image description in
`Atmosphere.cpp`, and are mirrored in `Common/atmosphere.part.glsl` for the
shaders. The hardcoded 192x108 and the `AP_SLICE_COUNT` / `AP_KM_PER_SLICE`
macros are gone.

Still hand-mirrored rather than generated: the two lists carry a "Must match"
comment and nothing in the build fails if they drift. Worth a generated header
if a third consumer ever appears.

### 11. Delete dead code [Partial]

Gone: the roughly 330 line `#if 0` block in `Atmosphere.cpp`,
`SingleScatteringResult::NewMultiScatStep0Out` and `Debug`, and the `ndcPos`
parameter that `sky-multiple-scattering.comp` accepted and ignored.

Also gone in the comment sweep: the commented-out depth buffer rubble in
`sky-multiple-scattering.comp`, the unused `uniformPhase` constants in
`sky-view-lut.frag` and `sky-camera-volumes.frag`, the dead `sunLuminance` local
and stale HLSL fragment in `sky-camera-volumes.frag`, and the
`FASTAERIALPERSPECTIVE_ENABLED` block, which item 5 turned into real code.

Still there, all inside the four `integrateScatteredRadiance` copies and so best
swept by item 9: the `tMaxMax` argument that `sky-ray-march.frag` overwrites on
its first line, the `mieRayPhase` argument that `sky-camera-volumes.frag` accepts
and ignores, `SingleScatteringResult::opticalDepth` (written in two shaders, read
in none), the dead `sampleCountFloor` / `tMaxFloor` locals in the two fixed step
marches, and the `clipSpace`, `tPrev` and `depthBufferValue = 0.0f` statements
that go nowhere. The bare `9000000.0f` sentinel also wants a name.

### 12. Skip LUT work when nothing changed [Skipped]

All four LUTs rebuild every frame. That is the design intent for dynamic
conditions, but the transmittance LUT depends only on the medium parameters and
the multiple scattering LUT only on those plus the sun zenith angle. Dirty
flagging both is straightforward once the parameters have a single owner, and it
matters as soon as the scene renders anything besides the sky.

## Already landed [Done]

- Full parameter ui with an `Atmosphere` window, authored values (scale heights,
  sun azimuth and elevation, sun colour and irradiance) folded into the uniform
  block by `applyAtmosphereSettings`.
- `exposure`, `sunAngularDiameterDegrees`, `sunDiskRadiance`,
  `multipleScatteringFactor`, `debugViewMode`, and `drawSunDisk` replace the
  hardcoded `L * 5`, `0.505` degrees, and `1e6` constants.
- `mieExtinction` is derived from scattering plus absorption rather than
  authored independently.
- Full screen debug visualizations of all four LUTs.

Nothing forces the default C++ struct layout to agree with std140. It currently
does, verified against the offsets `glslangValidator -q` reports for
`AtmosphereParamsBlock`, but nothing in the build checks it — re-run that
reflection after appending a field.

## References

- S. Hillaire, *A Scalable and Production Ready Sky and Atmosphere Rendering
  Technique*, Computer Graphics Forum 39(4), EGSR 2020. Reference implementation
  at [sebh/UnrealEngineSkyAtmosphere](https://github.com/sebh/UnrealEngineSkyAtmosphere).
- E. Bruneton and F. Neyret, *Precomputed Atmospheric Scattering*, Computer
  Graphics Forum 27(4), EGSR 2008. Source of the transmittance LUT
  parameterization and the density profiles.
- D. Hestroffer and C. Magnan, *Wavelength dependency of the Solar limb
  darkening*, Astronomy and Astrophysics 333, 338-342 (1998),
  [1998A&A...333..338H](https://ui.adsabs.harvard.edu/abs/1998A&A...333..338H).
  Equation 1 with `u = 1` is the sun disc power law; the per channel exponents
  `(0.397, 0.503, 0.652)` are their Table 2, Pierce and Slaughter solution, at
  679.1 nm, 552.2 nm and 443.9 nm.
- S. Hillaire, *Physically Based and Unified Volumetric Rendering in Frostbite*,
  SIGGRAPH 2015 course. Source of the energy conserving segment integration
  `(S - S * T) / sigma_t` that every march in these shaders uses.
