// scene_access.glsl -- GPU side of docs/GPU_LAYOUT.md (owner: 아미르).
// Include it after #version in any .frag that needs to ray-trace the scene:
//     #include "scene_access.glsl"
// Provides: data fetches, two-level BVH traversal (closest-hit and any-hit), hit decoding.
// Must stay in sync with src/scene/gpu_scene.{h,cpp}.

uniform sampler2D uVertPos;    // (position.xyz, uv.x)      1 texel / vertex
uniform sampler2D uVertAttr;   // (normal.xyz,   uv.y)      1 texel / vertex
uniform sampler2D uTriangles;  // (i0, i1, i2, materialId)  1 texel / triangle
uniform sampler2D uBlasNodes;  // BVH nodes, 2 texels each, all meshes concatenated
uniform sampler2D uTlasNodes;  // BVH nodes over instances, 2 texels each
uniform sampler2D uInstances;  // 4 texels: world->object rows 0..2, (blasRoot, triFirst, triCount, sceneIndex)
uniform sampler2D uMaterials;  // 3 texels per material
uniform int uTlasRoot;

const int   RTM_TEX_WIDTH = 2048;
const int   RTM_STACK_SIZE = 32;
const float RTM_INF = 1e30;

vec4 rtmFetch(sampler2D s, int i) {
    return texelFetch(s, ivec2(i % RTM_TEX_WIDTH, i / RTM_TEX_WIDTH), 0);
}

struct RtmHit {
    float t;        // RTM_INF = miss. Same t in world and object space (directions aren't renormalised).
    int   tri;      // absolute triangle index
    int   instance; // instance record index (TLAS order)
    vec2  bary;     // barycentrics of v1, v2
};

struct RtmSurface {
    vec3 position;     // world
    vec3 normal;       // world, interpolated shading normal, faces the incoming ray
    vec3 geomNormal;   // world, flat triangle normal, faces the incoming ray
    vec2 uv;
    int  material;
    bool frontFace;    // false = ray hit the back side (e.g. leaving glass)
};

struct RtmMaterial {
    vec3  baseColor;
    float metallic;
    vec3  emission;
    float roughness;
    float ior;
    float transmission;
    int   baseColorTex;  // tracer texture-array layer, -1 = none
    int   normalTex;
};

// ---------------------------------------------------------------------------

// Slab test. Returns entry distance, or RTM_INF on miss / beyond tMax.
float rtmIntersectAABB(vec3 o, vec3 invD, vec3 bmin, vec3 bmax, float tMax) {
    vec3 t0 = (bmin - o) * invD;
    vec3 t1 = (bmax - o) * invD;
    vec3 tmin = min(t0, t1), tmax = max(t0, t1);
    float tn = max(max(tmin.x, tmin.y), max(tmin.z, 0.0));
    float tf = min(min(tmax.x, tmax.y), min(tmax.z, tMax));
    return tn <= tf ? tn : RTM_INF;
}

// Möller–Trumbore, two-sided. Works with non-normalised d.
bool rtmIntersectTri(vec3 o, vec3 d, vec3 p0, vec3 p1, vec3 p2, float tMax, out float t, out vec2 bary) {
    vec3 e1 = p1 - p0, e2 = p2 - p0;
    vec3 pv = cross(d, e2);
    float det = dot(e1, pv);
    if (abs(det) < 1e-12) return false;
    float inv = 1.0 / det;
    vec3 tv = o - p0;
    float u = dot(tv, pv) * inv;
    if (u < 0.0 || u > 1.0) return false;
    vec3 qv = cross(tv, e1);
    float v = dot(d, qv) * inv;
    if (v < 0.0 || u + v > 1.0) return false;
    t = dot(e2, qv) * inv;
    bary = vec2(u, v);
    return t > 1e-6 && t < tMax;
}

vec3 rtmSafeInv(vec3 d) {
    const float eps = 1e-12;
    return 1.0 / vec3(abs(d.x) > eps ? d.x : eps, abs(d.y) > eps ? d.y : eps, abs(d.z) > eps ? d.z : eps);
}

// Traverses one mesh BLAS with an OBJECT-space ray. Updates hit if closer. Returns true on any hit.
bool rtmTraverseBlas(vec3 o, vec3 d, int root, int instance, bool anyHit, inout RtmHit hit) {
    vec3 invD = rtmSafeInv(d);
    bool found = false;

    vec4 ra = rtmFetch(uBlasNodes, root * 2), rb = rtmFetch(uBlasNodes, root * 2 + 1);
    if (rtmIntersectAABB(o, invD, ra.xyz, rb.xyz, hit.t) == RTM_INF) return false;

    int stack[RTM_STACK_SIZE];
    int sp = 0;
    int node = root;
    while (true) {
        vec4 a = rtmFetch(uBlasNodes, node * 2), b = rtmFetch(uBlasNodes, node * 2 + 1);
        int first = int(a.w), count = int(b.w);
        if (count > 0) {
            for (int i = first; i < first + count; ++i) {
                vec4 tri = rtmFetch(uTriangles, i);
                vec3 p0 = rtmFetch(uVertPos, int(tri.x)).xyz;
                vec3 p1 = rtmFetch(uVertPos, int(tri.y)).xyz;
                vec3 p2 = rtmFetch(uVertPos, int(tri.z)).xyz;
                float t; vec2 bc;
                if (rtmIntersectTri(o, d, p0, p1, p2, hit.t, t, bc)) {
                    hit.t = t; hit.tri = i; hit.instance = instance; hit.bary = bc;
                    found = true;
                    if (anyHit) return true;
                }
            }
        } else {
            int l = first, r = first + 1;
            float tl = rtmIntersectAABB(o, invD, rtmFetch(uBlasNodes, l * 2).xyz, rtmFetch(uBlasNodes, l * 2 + 1).xyz, hit.t);
            float tr = rtmIntersectAABB(o, invD, rtmFetch(uBlasNodes, r * 2).xyz, rtmFetch(uBlasNodes, r * 2 + 1).xyz, hit.t);
            if (tl > tr) { float tt = tl; tl = tr; tr = tt; int n = l; l = r; r = n; }  // near child first
            if (tl < RTM_INF) {
                if (tr < RTM_INF && sp < RTM_STACK_SIZE) stack[sp++] = r;
                node = l;
                continue;
            }
        }
        if (sp == 0) break;
        node = stack[--sp];
    }
    return found;
}

// Main entry: WORLD-space ray against the whole scene.
// anyHit = true for shadow rays (stops at the first hit, hit.t is then not the closest).
RtmHit rtmIntersectScene(vec3 o, vec3 d, float tMax, bool anyHit) {
    RtmHit hit;
    hit.t = tMax; hit.tri = -1; hit.instance = -1; hit.bary = vec2(0.0);
    vec3 invD = rtmSafeInv(d);

    int stack[RTM_STACK_SIZE];
    int sp = 0;
    int node = uTlasRoot;
    vec4 ra = rtmFetch(uTlasNodes, node * 2), rb = rtmFetch(uTlasNodes, node * 2 + 1);
    if (rtmIntersectAABB(o, invD, ra.xyz, rb.xyz, hit.t) == RTM_INF) { hit.t = RTM_INF; return hit; }

    while (true) {
        vec4 a = rtmFetch(uTlasNodes, node * 2), b = rtmFetch(uTlasNodes, node * 2 + 1);
        int first = int(a.w), count = int(b.w);
        if (count > 0) {
            for (int i = first; i < first + count; ++i) {
                vec4 r0 = rtmFetch(uInstances, i * 4);
                vec4 r1 = rtmFetch(uInstances, i * 4 + 1);
                vec4 r2 = rtmFetch(uInstances, i * 4 + 2);
                vec4 info = rtmFetch(uInstances, i * 4 + 3);
                vec3 oo = vec3(dot(r0.xyz, o) + r0.w, dot(r1.xyz, o) + r1.w, dot(r2.xyz, o) + r2.w);
                vec3 od = vec3(dot(r0.xyz, d), dot(r1.xyz, d), dot(r2.xyz, d));
                if (rtmTraverseBlas(oo, od, int(info.x), i, anyHit, hit) && anyHit) return hit;
            }
        } else {
            int l = first, r = first + 1;
            float tl = rtmIntersectAABB(o, invD, rtmFetch(uTlasNodes, l * 2).xyz, rtmFetch(uTlasNodes, l * 2 + 1).xyz, hit.t);
            float tr = rtmIntersectAABB(o, invD, rtmFetch(uTlasNodes, r * 2).xyz, rtmFetch(uTlasNodes, r * 2 + 1).xyz, hit.t);
            if (tl > tr) { float tt = tl; tl = tr; tr = tt; int n = l; l = r; r = n; }
            if (tl < RTM_INF) {
                if (tr < RTM_INF && sp < RTM_STACK_SIZE) stack[sp++] = r;
                node = l;
                continue;
            }
        }
        if (sp == 0) break;
        node = stack[--sp];
    }
    if (hit.tri < 0) hit.t = RTM_INF;
    return hit;
}

// Decodes a hit into world-space surface data.
RtmSurface rtmSurface(vec3 o, vec3 d, RtmHit hit) {
    RtmSurface s;
    vec4 tri = rtmFetch(uTriangles, hit.tri);
    int i0 = int(tri.x), i1 = int(tri.y), i2 = int(tri.z);
    vec4 p0 = rtmFetch(uVertPos, i0), p1 = rtmFetch(uVertPos, i1), p2 = rtmFetch(uVertPos, i2);
    vec4 a0 = rtmFetch(uVertAttr, i0), a1 = rtmFetch(uVertAttr, i1), a2 = rtmFetch(uVertAttr, i2);
    float w = 1.0 - hit.bary.x - hit.bary.y;

    // transpose(worldToObject) = inverse-transpose(objectToWorld): correct for non-uniform scale.
    vec4 r0 = rtmFetch(uInstances, hit.instance * 4);
    vec4 r1 = rtmFetch(uInstances, hit.instance * 4 + 1);
    vec4 r2 = rtmFetch(uInstances, hit.instance * 4 + 2);
    mat3 normalToWorld = mat3(r0.xyz, r1.xyz, r2.xyz);

    s.position   = o + d * hit.t;
    s.normal     = normalize(normalToWorld * (a0.xyz * w + a1.xyz * hit.bary.x + a2.xyz * hit.bary.y));
    s.geomNormal = normalize(normalToWorld * cross(p1.xyz - p0.xyz, p2.xyz - p0.xyz));
    s.uv         = vec2(p0.w, a0.w) * w + vec2(p1.w, a1.w) * hit.bary.x + vec2(p2.w, a2.w) * hit.bary.y;
    s.material   = int(tri.w);
    s.frontFace  = dot(s.geomNormal, d) < 0.0;
    if (!s.frontFace) s.geomNormal = -s.geomNormal;
    if (dot(s.normal, s.geomNormal) < 0.0) s.normal = -s.normal;
    return s;
}

RtmMaterial rtmMaterial(int id) {
    vec4 m0 = rtmFetch(uMaterials, id * 3);
    vec4 m1 = rtmFetch(uMaterials, id * 3 + 1);
    vec4 m2 = rtmFetch(uMaterials, id * 3 + 2);
    RtmMaterial m;
    m.baseColor = m0.rgb;  m.metallic = m0.a;
    m.emission  = m1.rgb;  m.roughness = m1.a;
    m.ior = m2.x;  m.transmission = m2.y;
    m.baseColorTex = int(m2.z);  m.normalTex = int(m2.w);
    return m;
}
