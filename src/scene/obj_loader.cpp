#include "scene/obj_loader.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

// 一个 f 顶点引用的三个下标（-1 表示这一项不存在）
struct FaceRef {
    int v = -1;
    int t = -1;
    int n = -1;

    bool operator==(const FaceRef& o) const { return v == o.v && t == o.t && n == o.n; }
};

struct FaceRefHash {
    size_t operator()(const FaceRef& r) const {
        // 三个小整数塞进一个 64 位里做哈希（下标远小于 2^21）
        uint64_t k = (uint64_t(uint32_t(r.v + 1)) << 42) ^ (uint64_t(uint32_t(r.t + 1)) << 21) ^
                     uint64_t(uint32_t(r.n + 1));
        k = (~k) + (k << 21);
        k = k ^ (k >> 24);
        k = (k + (k << 3)) + (k << 8);
        k = k ^ (k >> 14);
        k = (k + (k << 2)) + (k << 4);
        k = k ^ (k >> 28);
        return size_t(k + (k << 31));
    }
};

// "12/3/4"、"12//4"、"12/3"、"12" —— 缺的项保持 -1
bool parseFaceRef(const std::string& token, FaceRef& ref) {
    int values[3] = {0, 0, 0};
    int slot = 0;
    size_t start = 0;
    while (slot < 3) {
        const size_t slash = token.find('/', start);
        const std::string piece = token.substr(start, slash == std::string::npos ? std::string::npos
                                                                                 : slash - start);
        if (!piece.empty()) {
            char* end = nullptr;
            const long value = std::strtol(piece.c_str(), &end, 10);
            if (end == piece.c_str()) return false;
            values[slot] = int(value);
        }
        if (slash == std::string::npos) break;
        start = slash + 1;
        ++slot;
    }
    ref.v = values[0];
    ref.t = values[1];
    ref.n = values[2];
    return ref.v != 0;
}

// OBJ 的 1-based 正索引 / 负数表示"从末尾倒数"
int resolveIndex(int raw, size_t count) {
    const long idx = raw > 0 ? long(raw) - 1 : long(count) + long(raw);
    if (idx < 0 || idx >= long(count)) return -1;
    return int(idx);
}

}  // namespace

bool loadOBJ(const std::string& path, Mesh& out, std::string* error) {
    std::ifstream in(path);
    if (!in) {
        if (error) *error = "打不开文件: " + path;
        return false;
    }

    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> uvs;

    out.verts.clear();
    out.indices.clear();

    std::unordered_map<FaceRef, int, FaceRefHash> vertexMap;
    std::vector<int> face;

    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "v") {
            float x = 0, y = 0, z = 0;
            if (!(ss >> x >> y >> z)) {
                if (error) *error = "第 " + std::to_string(lineNo) + " 行 v 格式错误";
                return false;
            }
            positions.push_back({x, y, z});
        } else if (tag == "vn") {
            float x = 0, y = 0, z = 0;
            if (!(ss >> x >> y >> z)) {
                if (error) *error = "第 " + std::to_string(lineNo) + " 行 vn 格式错误";
                return false;
            }
            normals.push_back({x, y, z});
        } else if (tag == "vt") {
            float u = 0, v = 0;
            if (!(ss >> u)) {
                if (error) *error = "第 " + std::to_string(lineNo) + " 行 vt 格式错误";
                return false;
            }
            ss >> v;
            uvs.push_back({u, v});
        } else if (tag == "f") {
            face.clear();
            std::string token;
            while (ss >> token) {
                FaceRef ref;
                if (!parseFaceRef(token, ref)) {
                    if (error) *error = "第 " + std::to_string(lineNo) + " 行 f 引用格式错误: " + token;
                    return false;
                }
                ref.v = resolveIndex(ref.v, positions.size());
                if (ref.v < 0) {
                    if (error) *error = "第 " + std::to_string(lineNo) + " 行顶点下标越界";
                    return false;
                }
                ref.t = ref.t != 0 ? resolveIndex(ref.t, uvs.size()) : -1;
                ref.n = ref.n != 0 ? resolveIndex(ref.n, normals.size()) : -1;

                auto it = vertexMap.find(ref);
                if (it == vertexMap.end()) {
                    Vertex v;
                    v.position = positions[size_t(ref.v)];
                    v.normal = ref.n >= 0 ? normals[size_t(ref.n)] : Vec3{};
                    v.uv = ref.t >= 0 ? uvs[size_t(ref.t)] : Vec2{};
                    const int index = int(out.verts.size());
                    out.verts.push_back(v);
                    vertexMap.emplace(ref, index);
                    face.push_back(index);
                } else {
                    face.push_back(it->second);
                }
            }
            if (face.size() < 3) continue;
            // 扇形三角化：只对凸多边形正确，OBJ 里的面几乎都是这种
            for (size_t i = 1; i + 1 < face.size(); ++i) {
                out.indices.push_back(face[0]);
                out.indices.push_back(face[i]);
                out.indices.push_back(face[i + 1]);
            }
        }
        // o / g / s / usemtl / mtllib 一律忽略：D4 只关心几何
    }

    if (out.indices.empty()) {
        if (error) *error = "文件里没有面数据: " + path;
        return false;
    }

    // 没有 vn 的文件：用面法线补，每个三角形独立顶点（硬边）
    if (normals.empty()) {
        Mesh flat;
        flat.verts.reserve(out.indices.size());
        flat.indices.reserve(out.indices.size());
        for (size_t i = 0; i + 2 < out.indices.size(); i += 3) {
            const Vertex& a = out.verts[size_t(out.indices[i])];
            const Vertex& b = out.verts[size_t(out.indices[i + 1])];
            const Vertex& c = out.verts[size_t(out.indices[i + 2])];
            const Vec3 n = normalize(cross(b.position - a.position, c.position - a.position));
            const int base = int(flat.verts.size());
            flat.verts.push_back({a.position, n, a.uv});
            flat.verts.push_back({b.position, n, b.uv});
            flat.verts.push_back({c.position, n, c.uv});
            flat.indices.insert(flat.indices.end(), {base, base + 1, base + 2});
        }
        out = flat;
    }

    return true;
}
