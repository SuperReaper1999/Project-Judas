"""Authored Lastlight trip articulation for the original starter-pack skeleton.

These fields describe ordinary Judas bodies and passive joints on the animated
visual child. The CharacterMotor stays on its separate parent. The game decides
when a collision-aware shove becomes a trip; physics decides how this rig falls.
The shared soldier/zombie GLBs retain the same unique skeleton joint names.
"""

import json


# Parent-first mapped subset. Unmapped spine/neck/hand/foot joints retain the
# normal hierarchy; each physical parent is an actual skeleton ancestor.
# Shape offsets and dimensions are joint-local metres. The imported leg and
# arm segments point along their own +Y, including legs directed downward.
_BONES = (
    dict(joint="pelvis", parent="", offset=(0, .045, 0), half=(.145, .10, .09), mass=10),
    dict(joint="chest", parent="pelvis", offset=(0, .075, 0), half=(.185, .155, .09), mass=12),
    dict(joint="head", parent="chest", offset=(0, .12, 0), half=(.14, .14, .14), mass=3,
         shape=1, radius=.14, resistance=.8),
    dict(joint="upper_arm.L", parent="chest", offset=(0, .135, 0), half=(.055, .13, .065), mass=3),
    dict(joint="forearm.L", parent="upper_arm.L", offset=(0, .115, 0), half=(.048, .12, .05), mass=2,
         hinge=True, frame=(.9710319042, -.1251910776, -.1674498916, .1156928465)),
    dict(joint="upper_arm.R", parent="chest", offset=(0, .135, 0), half=(.055, .13, .065), mass=3),
    dict(joint="forearm.R", parent="upper_arm.R", offset=(0, .115, 0), half=(.048, .12, .05), mass=2,
         hinge=True, frame=(.9710319042, -.1251910776, .1674498916, -.1156928465)),
    dict(joint="thigh.L", parent="pelvis", offset=(0, .20, 0), half=(.07, .185, .07), mass=5),
    dict(joint="shin.L", parent="thigh.L", offset=(0, .225, 0), half=(.055, .21, .055), mass=3,
         hinge=True, frame=(.9996649027, .0116606727, -.0162494387, .0164351333)),
    dict(joint="thigh.R", parent="pelvis", offset=(0, .20, 0), half=(.07, .185, .07), mass=5),
    dict(joint="shin.R", parent="thigh.R", offset=(0, .225, 0), half=(.055, .21, .055), mass=3,
         hinge=True, frame=(.9996649027, .0116606727, .0162494387, -.0164351333)),
)

TRIP_JOINTS = tuple(bone["joint"] for bone in _BONES)
TRIP_ROOT = "pelvis"


def _value(value):
    if isinstance(value, str):
        return json.dumps(value)
    if isinstance(value, bool):
        return str(value).lower()
    if isinstance(value, (tuple, list)):
        return " ".join(str(x) for x in value)
    return str(value)


def ragdoll_fields(layer=0, mask=18446744073709551615):
    """Return named scene-authoring fields; caller adds animation/render/parent.

    All articulation bodies collide normally with the world, while intra-rig
    contact is explicitly disabled for this lightweight game content. Automatic
    anchors capture the CURRENT resolved skeletal pose on entry. Knees/elbows
    use X-axis hinges whose parent frames include the original child rest
    rotation; the other connections are ordinary passive ball joints. Explicit
    viscous resistance damps free rotation without freezing or teleporting bones.
    """
    result = ["  ragdoll.enabled true", "  ragdoll.play-on-start false",
              "  ragdoll.self-collision false", f"  ragdoll.bones {len(_BONES)}"]
    for index, bone in enumerate(_BONES):
        hinge = bone.get("hinge", False)
        fields = dict(
            joint=bone["joint"], parent=bone["parent"], shape=bone.get("shape", 0),
            offset=bone["offset"], orientation=(1, 0, 0, 0),
            **{"half-extents": bone["half"]}, radius=bone.get("radius", .06),
            mass=bone["mass"], friction=.7, restitution=.02,
            layer=layer, mask=mask,
            **{"suppress-parent": True, "auto-anchors": True},
            constraint=1 if hinge else 2, enabled=True,
            **{"anchor-a": (0, 0, 0), "anchor-b": (0, 0, 0),
               # WorldRagdoll's A endpoint is the child, B is its parent.
               "frame-a": (1, 0, 0, 0),
               "frame-b": bone.get("frame", (1, 0, 0, 0))},
            limits=hinge, lower=-.18, upper=2.15,
            **{"rotational-resistance": bone.get("resistance", 1.2)},
        )
        for key, value in fields.items():
            result.append(f"  ragdoll.bone.{index}.{key} {_value(value)}")
    return "\n".join(result) + "\n"
