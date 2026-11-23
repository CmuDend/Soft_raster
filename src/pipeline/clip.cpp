#include "pipeline/clip.h"

#include "math/vec.h"

VSOut lerpClip(const VSOut& a, const VSOut& b, float t) {
    VSOut o;
    o.clip = {a.clip.x + (b.clip.x - a.clip.x) * t, a.clip.y + (b.clip.y - a.clip.y) * t,
              a.clip.z + (b.clip.z - a.clip.z) * t, a.clip.w + (b.clip.w - a.clip.w) * t};
    o.worldPos = a.worldPos + (b.worldPos - a.worldPos) * t;
    o.normal = normalize(a.normal + (b.normal - a.normal) * t);
    o.uv = {a.uv.x + (b.uv.x - a.uv.x) * t, a.uv.y + (b.uv.y - a.uv.y) * t};
    return o;
}

int clipTriangleNear(const VSOut in[3], VSOut out[4]) {
    int count = 0;
    for (int i = 0; i < 3; ++i) {
        const VSOut& cur = in[i];
        const VSOut& nxt = in[(i + 1) % 3];

        const float dc = nearDist(cur);
        const float dn = nearDist(nxt);
        const bool inCur = dc >= 0.0f;
        const bool inNxt = dn >= 0.0f;

        // 当前顶点在内侧 → 保留
        if (inCur) out[count++] = cur;

        // 这条边跨过平面 → 补一个交点。inCur != inNxt 时 dc - dn 一定不为 0
        if (inCur != inNxt) {
            const float t = dc / (dc - dn);
            out[count++] = lerpClip(cur, nxt, t);
        }
    }
    return count;
}
