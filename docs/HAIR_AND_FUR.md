# Hair and Fur V2

Hair and Fur is an opt-in visual component on Experimental. A groom owns a
local growth patch, a material tint, and an optional set of fixed-step guides.
It can live on a moving object, an ordinary transform child, or a bone socket.
The character, animation and physics systems retain ownership of the actor.

## Author and preview a groom

1. Open the [Hair and Fur Lab](../projects/hair_fur_lab/README.md) or add an
   object to your own scene. In the Inspector, choose **Add component →
   Hair and fur**.
2. Choose **Animal coat**, **Mane**, **Tail**, **Human hair**, or **Beard** as
   a starting preset. Work in metres. Fit the patch centre and extents to the
   underlying surface. In the local frame, X and Z span the elliptical base
   and Y gives the dome height. Patch coverage sets the angle around that dome. Rotate the groom owner to place the patch.
3. For an animated model, add a **Visual socket** to the groom owner, select the
   character and choose a joint from the imported skeleton. Use the socket
   offset to fit the patch to the bone. Add separate patches for different
   attachment points. The Edit overlay shows the selected object's authored
   transform and guide layout. Use Play to see the live bone-resolved result.
4. Set the comb direction, comb strength, length, colour, width, variation and
   clumping. Set the coverage angle to wrap a coat and adjust the base coat
   for coverage or scalp volume. The wave control adds variation to strand
   curves. Long fibres blend the three closest guides, and the first part of
   each fibre follows the local comb to keep the roots coherent. The material uses a shared tangent highlight and ordinary scene
   lights and shadows.
5. For long grooms, add up to four local collision capsules for the head,
   shoulders or other relevant geometry. Equal endpoints define a sphere.
   An optional world collision sweep tests the tip of each guide against the
   current physics world. Leave it off for grooms that only need owner
   collision. Enter Play and check the scalp, collars and attachment in
   motion before exporting.

Save and undo edits through the regular scene workflow. A groom in a prefab
travels with its scene-object data. Existing imported hair cards and meshes
still use the usual render, material and skinning paths. Use them alongside
procedural patches or as the base volume of a coiffure.

## Simulation and scalability

Each simulated groom uses 4–32 guides with 2–8 segments. It keeps previous
and current positions for presentation interpolation. Inertia comes from the
moving resolved attachment frame while guide points integrate entity gravity
and the local weather wind. Damping, stiffness, iterative segment limits and
a bounded displacement per step keep the model stable. Large discontinuities
reset guides to their attachment. Floating-origin shifts translate transient
samples together.

The 4–512 rendered fibres are a stable seeded distribution and can outnumber
the guides. Short-coat shells and crossed fibre ribbons use an indexed vertex stream
and a single material submission per groom per pass. Distance reduces the
visible fibre count between the authored LOD distances. The closest
participating gameplay or target camera selects the shared density. The
existing graphics quality decorative density affects rendered fibres and their
shadows. Guide count, fixed-step work and gameplay state are independent.
Simulated grooms continue advancing outside rendering distance while their
region remains published.

Local capsules affect guide nodes. World collision only sweeps guide tips once
per guide per fixed step. Rendered children, full strand-to-strand contact,
and interaction forces back onto actors are outside this model. Clump
settings converge rendered children toward guides. Keep roots outside body
proxies and allow enough space for the intended segment length. Tight contact
and rapid rotation need visual checks, particularly with a low guide count.

## Scene, save and backend contract

The hair.groom property is an inline version-1 JSON descriptor carried by the
ordinary scene and prefab object format. It uses the owner transform and
optional existing socket reference. The groom has no generated asset files
to register. The maintained lab includes imported skinned models for the
horse and performer. The SDK ships the scene and its dependent assets.

Saving retains the authored patch and socket. Derived cosmetic guides start
at rest on a cold load. Region suspension clears transient guides.
OpenGL–Vulkan reconstruction retains CPU guide samples and rebuilds the derived
graphics. Generated geometry follows the renderer's ordinary render-layer and
shadow-pass policies. Render scale and camera-target changes leave physics
unchanged.

Groom authoring currently uses explicit patches and sockets. Surface painting,
per-root skin weighting, standalone groom assets and per-fibre transparency
are outside this increment. The lab and the hair-groom validation case provide
automated evidence. Human visual acceptance is separate.
