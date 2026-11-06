#pragma once
// The exhibition hallway: architecture + one slot per exhibit.
//
//                 far wall (z = -7)
//          ┌──────────────────────────┐
//          │         [arcade]         │  z = -6.2  floor-standing, faces the entrance
//          │                          │
//   left   │ [CRT TV]       [radio]   │  z = -3    pedestals at x = ±1.5,
//   wall   │ [telephone]    [boombox] │  z =  0    exhibits face the walkway
//   x=-2.2 │ [camera]   [typewriter]  │  z = +3
//          │                          │
//          │           you            │  camera starts at z = +6, looking -Z
//          └──────────────────────────┘
//                 entrance (z = +7)
//
// Until the OBJ loader exists (하리스), every slot shows a placeholder box of roughly the
// real object's size. Real model: load kExhibits[i].objPath, then fit it into the slot
// with fitToSlot() so it sits on the pedestal at the right scale.
#include <array>

#include "scene/scene.h"

namespace rtm {

struct ExhibitSlot {
    const char* name;
    const char* objPath;       // relative to assets/ -- where the converted model must go
    glm::vec3 size;            // approximate real size in metres (w, h, d); placeholder + fit target
    glm::vec3 position;        // world position of the object's bottom centre
    float yawDeg;              // which way its front (+Z) faces
    bool onPedestal;
};

constexpr float kPedestalHeight = 1.0f;
constexpr float kPedestalSize = 0.6f;
constexpr float kHallHalfWidth = 2.2f;
constexpr float kHallHalfLength = 7.0f;
constexpr float kHallHeight = 3.5f;

// Order = exhibit index used everywhere (ImGui, report). Edit positions here only.
extern const std::array<ExhibitSlot, 7> kExhibits;

Scene makeGallery();

// Uniform scale + translation that drops a mesh with object-space `bounds` into `slot`:
// largest dimension fitted to the slot, bottom centre on the slot position.
Transform fitToSlot(const AABB& bounds, const ExhibitSlot& slot);

}  // namespace rtm
