#include "scene/gallery.h"

#include <algorithm>
#include <string>

#include <glm/gtc/quaternion.hpp>

namespace rtm {

namespace {

constexpr float kRowX = 1.5f;       // pedestal centre distance from the hallway centre line
constexpr float kLeftYaw = 90.0f;   // +Z front turned to face +X (the walkway)
constexpr float kRightYaw = -90.0f;

}  // namespace

// Sizes are rough real-world dimensions; fitToSlot() scales the real model to the largest one.
const std::array<ExhibitSlot, 7> kExhibits = {{
    {"Film camera",    "models/camera/camera.obj",         {0.16f, 0.10f, 0.10f}, {-kRowX, kPedestalHeight,  3.0f}, kLeftYaw,  true},
    {"Telephone",      "models/telephone/telephone.obj",   {0.28f, 0.48f, 0.16f}, {-kRowX, kPedestalHeight,  0.0f}, kLeftYaw,  true},
    {"CRT television", "models/television/television.obj", {0.50f, 0.40f, 0.45f}, {-kRowX, kPedestalHeight, -3.0f}, kLeftYaw,  true},
    {"Typewriter",     "models/typewriter/typewriter.obj", {0.45f, 0.22f, 0.40f}, { kRowX, kPedestalHeight,  3.0f}, kRightYaw, true},
    {"Boombox",        "models/boombox/boombox.obj",       {0.60f, 0.32f, 0.16f}, { kRowX, kPedestalHeight,  0.0f}, kRightYaw, true},
    {"Tube radio",     "models/radio/radio.obj",           {0.50f, 0.35f, 0.26f}, { kRowX, kPedestalHeight, -3.0f}, kRightYaw, true},
    {"Arcade cabinet", "models/arcade/arcade.obj",         {0.70f, 1.80f, 0.85f}, { 0.0f,  0.0f,           -6.2f}, 0.0f,      false},
}};

namespace {

// Appends a box as a new submesh. inward = normals point into the box (room shell).
// faceMask bit order: +Z, -Z, +X, -X, +Y, -Y.
void addBox(Mesh& mesh, glm::vec3 mn, glm::vec3 mx, bool inward, int material, unsigned faceMask = 0x3F) {
    const glm::vec3 c[8] = {
        {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mx.x, mx.y, mn.z}, {mn.x, mx.y, mn.z},
        {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z}, {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z},
    };
    struct Face { int a, b, cc, d; glm::vec3 n; };
    const Face faces[6] = {
        {4, 5, 6, 7, {0, 0, 1}},  {1, 0, 3, 2, {0, 0, -1}}, {5, 1, 2, 6, {1, 0, 0}},
        {0, 4, 7, 3, {-1, 0, 0}}, {7, 6, 2, 3, {0, 1, 0}},  {0, 1, 5, 4, {0, -1, 0}},
    };
    const glm::vec2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

    SubMesh sm;
    sm.firstIndex = static_cast<uint32_t>(mesh.indices.size());
    sm.material = material;
    for (int f = 0; f < 6; ++f) {
        if (!(faceMask & (1u << f))) continue;
        const Face& face = faces[f];
        const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
        const int ids[4] = {face.a, face.b, face.cc, face.d};
        for (int i = 0; i < 4; ++i) {
            Vertex v;
            v.position = c[ids[i]];
            v.normal = inward ? -face.n : face.n;
            v.uv = uv[i];
            mesh.vertices.push_back(v);
        }
        if (!inward) mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        else         mesh.indices.insert(mesh.indices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
    }
    sm.indexCount = static_cast<uint32_t>(mesh.indices.size()) - sm.firstIndex;
    mesh.submeshes.push_back(sm);
}

int addMaterial(Scene& s, const char* name, glm::vec3 color, float roughness, float metallic = 0.0f) {
    Material m;
    m.name = name;
    m.baseColor = color;
    m.roughness = roughness;
    m.metallic = metallic;
    s.materials.push_back(m);
    return static_cast<int>(s.materials.size() - 1);
}

int addMesh(Scene& s, Mesh m) {
    m.computeBounds();
    s.meshes.push_back(std::move(m));
    return static_cast<int>(s.meshes.size() - 1);
}

}  // namespace

Scene makeGallery() {
    Scene s;
    const int floorMat    = addMaterial(s, "floor", {0.30f, 0.24f, 0.18f}, 0.6f);
    const int wallMat     = addMaterial(s, "wall", {0.62f, 0.56f, 0.48f}, 0.9f);
    const int pedestalMat = addMaterial(s, "pedestal", {0.85f, 0.82f, 0.75f}, 0.7f);
    const int bodyMat     = addMaterial(s, "placeholder body", {0.12f, 0.35f, 0.40f}, 0.4f);
    const int frontMat    = addMaterial(s, "placeholder front", {0.80f, 0.45f, 0.15f}, 0.3f, 1.0f);

    // Hallway shell (floor separate so it gets its own material).
    Mesh hall;
    hall.name = "hallway";
    const glm::vec3 hmin(-kHallHalfWidth, 0.0f, -kHallHalfLength), hmax(kHallHalfWidth, kHallHeight, kHallHalfLength);
    addBox(hall, hmin, hmax, true, floorMat, 1u << 5);
    addBox(hall, hmin, hmax, true, wallMat, 0x1F);
    s.instances.push_back({"hallway", addMesh(s, std::move(hall)), {}, false});

    // One shared pedestal mesh, origin at bottom centre.
    Mesh ped;
    ped.name = "pedestal";
    const float h = kPedestalSize * 0.5f;
    addBox(ped, {-h, 0.0f, -h}, {h, kPedestalHeight, h}, false, pedestalMat);
    const int pedMesh = addMesh(s, std::move(ped));

    for (const ExhibitSlot& slot : kExhibits) {
        if (slot.onPedestal) {
            Instance p{std::string("pedestal: ") + slot.name, pedMesh, {}, false};
            p.transform.position = {slot.position.x, 0.0f, slot.position.z};
            s.instances.push_back(p);
        }
        // Placeholder: box of the slot's size, orange +Z face = the object's front.
        // TODO(하리스): replace with the loaded OBJ + fitToSlot(mesh.bounds, slot).
        Mesh box;
        box.name = std::string("placeholder: ") + slot.name;
        const glm::vec3 e = slot.size * 0.5f;
        addBox(box, {-e.x, 0.0f, -e.z}, {e.x, slot.size.y, e.z}, false, bodyMat, 0x3F & ~1u);
        addBox(box, {-e.x, 0.0f, -e.z}, {e.x, slot.size.y, e.z}, false, frontMat, 1u);
        const int boxMesh = addMesh(s, std::move(box));

        Instance ex{slot.name, boxMesh, {}, true};
        ex.transform.position = slot.position;
        ex.transform.rotation = glm::angleAxis(glm::radians(slot.yawDeg), glm::vec3(0, 1, 0));
        s.instances.push_back(ex);
    }
    return s;
}

Transform fitToSlot(const AABB& b, const ExhibitSlot& slot) {
    Transform t;
    const glm::vec3 ext = b.extent();
    const float modelMax = std::max({ext.x, ext.y, ext.z, 1e-6f});
    const float slotMax = std::max({slot.size.x, slot.size.y, slot.size.z});
    const float scale = slotMax / modelMax;
    t.scale = glm::vec3(scale);
    t.rotation = glm::angleAxis(glm::radians(slot.yawDeg), glm::vec3(0, 1, 0));
    // Move the model's bottom centre to the origin, then onto the slot.
    const glm::vec3 bottomCentre(b.center().x, b.min.y, b.center().z);
    t.position = slot.position - t.rotation * (bottomCentre * scale);
    return t;
}

}  // namespace rtm
