#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <windows.h>
#include <stdint.h>
#include <math.h>
#include <algorithm>

LARGE_INTEGER g_freq;

void InitTimer()
{
    QueryPerformanceFrequency(&g_freq);
}
static LARGE_INTEGER last = {};
float GetDeltaTime()
{
    LARGE_INTEGER now;

    QueryPerformanceCounter(&now);

    if (last.QuadPart == 0) {
        last = now;
        return 0.0f;
    }

    float dt = (float)(now.QuadPart - last.QuadPart) / (float)g_freq.QuadPart;
    last = now;

    return dt;
}

float clamp(float x, float minVal, float maxVal)
{
    if (x < minVal) return minVal;
    if (x > maxVal) return maxVal;
    return x;
}

static const int WIDTH  = 640;
static const int HEIGHT = 360;
float fov = 90.0f * 3.1415926f / 180.0f;
float aspect = WIDTH/1.0/HEIGHT;
double minZ, maxZ;

uint32_t* framebuffer = nullptr;
HBITMAP dib = nullptr;
HDC memDC = nullptr;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

struct Vec2 {
    float x, y;
};

struct Vec2Z {
    float x, y;
    float zpos;
};

struct Vec3 {
    float x, y, z;
};

struct Color {
    float r,g,b;
};
Color operator+(Color a, Color b){
    return {a.r+b.r, a.g+b.g, a.b+b.b};
}
Color operator*(Color a, float b){
    return {a.r*b, a.g*b, a.b*b};
}
Color operator*(Color a, Color b){
    return {a.r*b.r, a.g*b.g, a.b*b.b};
}
Color operator/(Color a, float b){
    return {a.r/b, a.g/b, a.b/b};
}

struct vertex {
    Vec3 pos;
    Vec3 normal;
};

Vec3 operator+(Vec3 a, Vec3 b){
    return {a.x+b.x, a.y+b.y, a.z+b.z};
}

Vec3 operator-(Vec3 a, Vec3 b){
    return {a.x-b.x, a.y-b.y, a.z-b.z};
}

vertex operator-(vertex a, vertex b){
    return {a.pos-b.pos, a.normal};
}

Vec3 operator-(Vec3 a){
    return {-a.x, -a.y, -a.z};
}

Vec3 operator-(Vec3 a, float t){
    return {a.x-t, a.y-t, a.z-t};
}

Vec3 operator*(Vec3 a, float t){
    return {a.x*t, a.y*t, a.z*t};
}

Vec3 operator/(Vec3 a, float t){
    return {a.x/t, a.y/t, a.z/t};
}

Vec3 cross(Vec3 a, Vec3 b){
    return {a.y*b.z - a.z*b.y,a.z*b.x - a.x*b.z,a.x*b.y - a.y*b.x};
}
float dot(Vec3 a, Vec3 b){
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

Vec3 normalize(Vec3 v)
{
    float len = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
    return { v.x/len, v.y/len, v.z/len };
}

struct Vec4 {
    float x, y, z, w;
};

struct triangleP {
    int a, b, c;
};

struct triangle {
    vertex a, b, c;
    Vec3 normal;
    Vec3 edgeA, edgeB;
    float mirror = 0.0f;
    float diffuse = 0.0f;
    float transparency = 0.0f;
};

struct Tri3 {
    Vec3 a, b, c;
};

std::vector<Tri3> sampleCube = {
    {{0,0,1},{1,0,1},{1,1,1}},
    {{0,0,1},{1,1,1},{0,1,1}},

    {{0,0,0},{1,1,0},{1,0,0}},
    {{0,0,0},{0,1,0},{1,1,0}},

    {{0,0,0},{0,0,1},{0,1,1}},
    {{0,0,0},{0,1,1},{0,1,0}},

    {{1,0,0},{1,1,1},{1,0,1}},
    {{1,0,0},{1,1,0},{1,1,1}},

    {{0,1,0},{0,1,1},{1,1,1}},
    {{0,1,0},{1,1,1},{1,1,0}},

    {{0,0,0},{1,0,1},{0,0,1}},
    {{0,0,0},{1,0,0},{1,0,1}},
};

struct TriZ {
    Vec2Z a, b, c;
};

Vec3 TriangleNormal(const Tri3& t)
{
    Vec3 e1 = { t.b.x - t.a.x, t.b.y - t.a.y, t.b.z - t.a.z };
    Vec3 e2 = { t.c.x - t.a.x, t.c.y - t.a.y, t.c.z - t.a.z };
    return normalize(cross(e1, e2));
}

struct Mat4 {
    float m[4][4];
};

struct Camera {
    Vec3 pos;
    float yaw;
    float pitch;
};

struct Ray {
    Vec3 origin;
    Vec3 dir;
};

std::vector <vertex> verts(0);
std::vector <vertex> tempverts(0);
std::vector <triangle> tris(0);
std::vector <uint32_t> colors(0);

std::vector <Vec3> lights(0);
std::vector <uint32_t> lcolors(0);
std::vector <float> lintensity(0);


Mat4 Identity()
{
    Mat4 r = {};
    r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0f;
    return r;
}

Mat4 operator*(const Mat4& a, const Mat4& b)
{
    Mat4 r = {};

    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
                r.m[i][j] += a.m[i][k] * b.m[k][j];

    return r;
}

Vec4 operator*(const Mat4& m, const Vec4& v)
{
    return {
        m.m[0][0]*v.x + m.m[0][1]*v.y + m.m[0][2]*v.z + m.m[0][3]*v.w,
        m.m[1][0]*v.x + m.m[1][1]*v.y + m.m[1][2]*v.z + m.m[1][3]*v.w,
        m.m[2][0]*v.x + m.m[2][1]*v.y + m.m[2][2]*v.z + m.m[2][3]*v.w,
        m.m[3][0]*v.x + m.m[3][1]*v.y + m.m[3][2]*v.z + m.m[3][3]*v.w
    };
}

Mat4 Translation(Vec3 t)
{
    Mat4 r = Identity();
    r.m[0][3] = t.x;
    r.m[1][3] = t.y;
    r.m[2][3] = t.z;
    return r;
}

Mat4 RotationY(float a)
{
    Mat4 r = Identity();
    float c = cosf(a);
    float s = sinf(a);

    r.m[0][0] =  c;
    r.m[0][2] =  s;
    r.m[2][0] = -s;
    r.m[2][2] =  c;
    return r;
}

Mat4 RotationX(float a)
{
    Mat4 r = Identity();
    float c = cosf(a);
    float s = sinf(a);

    r.m[1][1] =  c;
    r.m[1][2] = -s;
    r.m[2][1] =  s;
    r.m[2][2] =  c;
    return r;
}

Mat4 Perspective(float fov, float aspect, float zn, float zf)
{
    Mat4 r = {};
    float f = 1.0f / tanf(fov * 0.5f);

    r.m[0][0] = f / aspect;
    r.m[1][1] = f;
    r.m[2][2] = zf / (zf - zn);
    r.m[2][3] = (-zf * zn) / (zf - zn);
    r.m[3][2] = 1.0f;

    return r;
}

Mat4 GetViewMatrix(const Camera& c)
{
    Mat4 rotX = RotationX(-c.pitch);
    Mat4 rotY = RotationY(-c.yaw);
    Mat4 trans = Translation({ -c.pos.x, -c.pos.y, -c.pos.z });

    return rotX * rotY * trans;
}

float zbuffer[WIDTH * HEIGHT];

Vec2 Project(Vec3 v)
{
    float f = 1.0f / tanf(fov * 0.5f);

    Vec2 out;
    out.x = (v.x * f) / v.z;
    out.y = (v.y * f) / v.z;
    return out;
}

Vec2Z ProjectZ(Vec3 v)
{
    float f = 1.0f / tanf(fov * 0.5f);

    Vec2Z out;
    out.x = (v.x * f) / v.z;
    out.y = (v.y * f) / v.z;
    out.zpos = 1.0f / v.z;

    return out;
}

TriZ ProjectTriangle(Tri3 t){
    return {ProjectZ(t.a), ProjectZ(t.b), ProjectZ(t.c)};
}

Vec3 ViewSpace(Vec3 vi, const Camera& cam){
    Vec4 v = { vi.x, vi.y, vi.z, 1.0f };
    Mat4 View = GetViewMatrix(cam);
    Vec4 noclip = View * v;
    return {noclip.x, noclip.y, noclip.z};
}

Vec3 ViewSpaceDir(Vec3 vi, const Camera& cam){
    Vec4 v = { vi.x, vi.y, vi.z, 0.0f };
    Mat4 View = GetViewMatrix(cam);
    Vec4 noclip = View * v;
    return {noclip.x, noclip.y, noclip.z};
}


Vec3 TransformAll(Vec3 vi)
{
    Mat4 Proj = Perspective(fov, aspect, 0.1f, 1000.0f);

    Vec4 v = { vi.x, vi.y, vi.z, 1.0f };

    Vec4 clip = Proj * v;

    Vec3 ndc = {
        clip.x / clip.w,
        clip.y / clip.w,
        clip.z / clip.w
    };

    float screenX = (ndc.x * 0.5f + 0.5f) * WIDTH;
    float screenY = (1.0f - (ndc.y * 0.5f + 0.5f)) * HEIGHT;
    return {screenX, screenY, ndc.z};
}

vertex IntersectNearPlane(const vertex& in, const vertex& out, float nearZ)
{
    float t = (nearZ - in.pos.z) / (out.pos.z - in.pos.z);
    vertex v;
    v.pos = {
        in.pos.x + t * (out.pos.x - in.pos.x),
        in.pos.y + t * (out.pos.y - in.pos.y),
        nearZ
    };

    v.normal = {
        in.normal.x + t * (out.normal.x - in.normal.x),
        in.normal.y + t * (out.normal.y - in.normal.y),
        in.normal.z + t * (out.normal.z - in.normal.z)
    };

    v.normal = normalize(v.normal);
    return v;
}

void ClipTriangleNearPlane(
    const triangle& tri,
    float nearZ,
    std::vector<triangle>& out)
{
    const vertex& v0 = tri.a;
    const vertex& v1 = tri.b;
    const vertex& v2 = tri.c;

    bool i0 = v0.pos.z >= nearZ;
    bool i1 = v1.pos.z >= nearZ;
    bool i2 = v2.pos.z >= nearZ;

    int insideCount = (int)i0 + (int)i1 + (int)i2;

    if (insideCount == 0)
        return;

    if (insideCount == 3) {
        out.push_back(tri);
        return;
    }

    if (insideCount == 1) {
        vertex in, out1, out2;

        if (i0) { in = v0; out1 = v1; out2 = v2; }
        else if (i1) { in = v1; out1 = v2; out2 = v0; }
        else { in = v2; out1 = v0; out2 = v1; }

        vertex iA = IntersectNearPlane(in, out1, nearZ);
        vertex iB = IntersectNearPlane(in, out2, nearZ);

        out.push_back({ in, iA, iB });
        return;
    }

    if (insideCount == 2) {
        vertex in1, in2, outV;

        if (!i0) { outV = v0; in1 = v1; in2 = v2; }
        else if (!i1) { outV = v1; in1 = v2; in2 = v0; }
        else { outV = v2; in1 = v0; in2 = v1; }

        vertex iA = IntersectNearPlane(in1, outV, nearZ);
        vertex iB = IntersectNearPlane(in2, outV, nearZ);

        out.push_back({ in1, in2, iA });
        out.push_back({ in2, iB, iA });
        return;
    }
}

void NormalizeCamera(Camera& c)
{
    const float MAX_PITCH = 1.55334f;
    if (c.pitch >  MAX_PITCH) c.pitch =  MAX_PITCH;
    if (c.pitch < -MAX_PITCH) c.pitch = -MAX_PITCH;

    const float PI = 3.1415926f;
    const float TWO_PI = PI * 2.0f;

    if (c.yaw >  PI) c.yaw -= TWO_PI;
    if (c.yaw < -PI) c.yaw += TWO_PI;
}

inline float edge(float ax, float ay, float bx, float by, float cx, float cy)
{
    return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
}

void Clear(uint32_t color)
{
    for (int i = 0; i < WIDTH * HEIGHT; i++)
        framebuffer[i] = color;
}

void ClearZ()
{
    for (int i = 0; i < WIDTH * HEIGHT; i++)
        zbuffer[i] = -1e30f;
}

void DrawTriangle(
    float x0, float y0, float z0,
    float x1, float y1, float z1,
    float x2, float y2, float z2, Vec3 n0, Vec3 n1, Vec3 n2, Vec3 light,
    uint32_t color)
{
    int minX = (int)fmaxf(0, floorf(fminf(x0, fminf(x1, x2))));
    int minY = (int)fmaxf(0, floorf(fminf(y0, fminf(y1, y2))));
    int maxX = (int)fminf(WIDTH-1,  ceilf(fmaxf(x0, fmaxf(x1, x2))));
    int maxY = (int)fminf(HEIGHT-1, ceilf(fmaxf(y0, fmaxf(y1, y2))));


    float area = edge(x0, y0, x1, y1, x2, y2);
    if (area == 0) return;
    float invarea = 1.0f/area;

    bool topLeft = area > 0;

    float e0_dx = y2 - y1, e0_dy = x1 - x2;
    float e1_dx = y0 - y2, e1_dy = x2 - x0;
    float e2_dx = y1 - y0, e2_dy = x0 - x1;

    float baseX = (float)minX + 0.05f;
    float baseY = (float)minY + 0.05f;

    float w0_row = edge(x1, y1, x2, y2, baseX, baseY);
    float w1_row = edge(x2, y2, x0, y0, baseX, baseY);
    float w2_row = edge(x0, y0, x1, y1, baseX, baseY);

    for (int y = minY; y <= maxY; y++)
    {
        float w0 = w0_row;
        float w1 = w1_row;
        float w2 = w2_row;
        for (int x = minX; x <= maxX; x++)
        {
            if (w0 >= 0 && w1 >= 0 && w2 >= 0 && area > 0 ||
                w0 <= 0 && w1 <= 0 && w2 <= 0 && area < 0){
                float w0n = w0 / area;
                float w1n = w1 / area;
                float w2n = w2 / area;

                double invZ = w0n * z0 + w1n * z1 + w2n * z2;

                Vec3 n = {
                    w0n * n0.x + w1n * n1.x + w2n * n2.x,
                    w0n * n0.y + w1n * n1.y + w2n * n2.y,
                    w0n * n0.z + w1n * n1.z + w2n * n2.z
                };

                float lambert = dot(n, light);
                lambert = std::max(0.0f, lambert);

                float shade = std::fminf(invZ*2, 1.0);

                int idx = y * WIDTH + x;
                if (invZ > zbuffer[idx] && invZ > 0 && invZ < 1)
                {
                    zbuffer[idx] = invZ;
                    int colorR = ((color&0x00FF0000)>>16)*shade;
                    int colorG = ((color&0x0000FF00)>>8)*shade;
                    int colorB = (color&0x000000FF)*shade;
                    framebuffer[idx] = colorR*65536+colorG*256+colorB;
                    minZ = std::min(minZ, invZ);
                    maxZ = std::max(maxZ, invZ);
                }
            }
            w0 += e0_dx;
            w1 += e1_dx;
            w2 += e2_dx;
        }
        w0_row += e0_dy;
        w1_row += e1_dy;
        w2_row += e2_dy;
    }
}

uint32_t darken(uint32_t color, float b)
{
    int r = ((color >> 16) & 255) * b;
    int g = ((color >> 8) & 255) * b;
    int bcol = (color & 255) * b;

    return (r << 16) | (g << 8) | bcol;
}

Vec3 GetForward(const Camera& c)
{
    return {
        cosf(c.pitch) * sinf(c.yaw),
        sinf(c.pitch),
        cosf(c.pitch) * cosf(c.yaw)
    };
}

Vec3 GetRight(const Camera& c)
{
    return {
        cosf(c.yaw),
        0.0f,
        -sinf(c.yaw)
    };
}

Vec3 GetUp(const Camera& c)
{
    Vec3 forward = GetForward(c);
    Vec3 right   = GetRight(c);

    return normalize(cross(right, forward));
}

void MoveCamera(Camera& c, float forward, float strafe, float dt, float speed)
{
    Vec3 f = GetForward(c);
    Vec3 r = GetRight(c);

    if (dt > 10) dt = 1;

    float s = speed * dt;

    c.pos.x += (f.x * forward + r.x * strafe) * s;
    c.pos.y += -(f.y * forward) * s;
    c.pos.z += (f.z * forward + r.z * strafe) * s;
}

bool LoadOBJTriangles(const std::string& filename, std::vector<vertex>& vertices, std::vector<triangle>& triangles)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    std::vector<triangleP> trianglesP;

    vertices.clear();
    triangles.clear();

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "v") {
            vertex v;
            ss >> v.pos.x >> v.pos.y >> v.pos.z;
            v.normal = {0,0,0};
            vertices.push_back(v);
        } else if (tag == "f") {
            std::string vertStr;
            std::vector<int> indices;

            while (ss >> vertStr) {
                size_t slash1 = vertStr.find('/');
                int idx = std::stoi(vertStr.substr(0, slash1));
                if (idx < 0) idx = (int)vertices.size() + idx;
                else idx = idx - 1;
                indices.push_back(idx);
            }

            for (size_t i = 1; i + 1 < indices.size(); i++) {
                triangleP tr;
                tr.a = indices[0];
                tr.b = indices[i];
                tr.c = indices[i + 1];

                Vec3 faceNormal = TriangleNormal({vertices[tr.a].pos, vertices[tr.b].pos, vertices[tr.c].pos});
                vertices[indices[0]].normal = vertices[indices[0]].normal + faceNormal;
                vertices[indices[i]].normal = vertices[indices[i]].normal + faceNormal;
                vertices[indices[i+1]].normal = vertices[indices[i+1]].normal + faceNormal;

                trianglesP.push_back(tr);
            }
        }
    }
    for (auto& v : vertices)
        v.normal = normalize(v.normal);
    for (auto& t : trianglesP){
        triangles.push_back({{vertices[t.a].pos, vertices[t.a].normal}, {vertices[t.b].pos, vertices[t.b].normal}, {vertices[t.c].pos, vertices[t.c].normal}});
        int b = triangles.size()-1;
        triangles[b].normal = normalize(cross(triangles[b].b.pos - triangles[b].a.pos, triangles[b].c.pos - triangles[b].a.pos));
        triangles[b].edgeA = triangles[b].b.pos - triangles[b].a.pos;
        triangles[b].edgeB = triangles[b].c.pos - triangles[b].a.pos;
    }
    return true;
}

bool LoadFromXSCENE(const std::string& filename){
    std::ifstream fin(filename);
    if (!fin.is_open()) {
        return false;
    }
    tris.clear();
    colors.clear();
    lights.clear();
    lcolors.clear();
    lintensity.clear();

    int n = 0;
    fin >> n;
    for (int i=0;i<n;i++){
        Vec3 a, b, c;
        int rr,gg,bb;
        float mirr, diff;
        fin >> a.x >> a.y >> a.z;
        fin >> b.x >> b.y >> b.z;
        fin >> c.x >> c.y >> c.z;
        fin >> rr >> gg >> bb;
        fin >> mirr >> diff;
        tris.push_back({{a, {0,0,0}}, {b, {0,0,0}}, {c, {0,0,0}}});
        int tp = tris.size()-1;
        tris[tp].normal = normalize(cross(tris[tp].b.pos - tris[tp].a.pos, tris[tp].c.pos - tris[tp].a.pos));
        tris[tp].a.normal = tris[tp].normal;
        tris[tp].b.normal = tris[tp].normal;
        tris[tp].c.normal = tris[tp].normal;
        tris[tp].edgeA = tris[tp].b.pos - tris[tp].a.pos;
        tris[tp].edgeB = tris[tp].c.pos - tris[tp].a.pos;
        tris[tp].mirror = mirr;
        tris[tp].diffuse = diff;
        colors.push_back(rr*256*256+gg*256+bb);
    }
    fin >> n;
    for (int i=0;i<n;i++){
        Vec3 pos;
        int r,g,b;
        float intensity;
        fin >> pos.x >> pos.y >> pos.z;
        fin >> r >> g >> b;
        fin >> intensity;
        lights.push_back(pos);
        lcolors.push_back(r*256*256+g*256+b);
        lintensity.push_back(intensity);
    }
    fin.close();
    return true;
}
bool WriteToXSCENE(const std::string& filename){
    std::ofstream fout(filename);
    if (!fout.is_open()) {
        return false;
    }

    int n = tris.size();
    fout << n;
    for (int i=0;i<n;i++){
        fout << tris[i].a.pos.x << ' ' << tris[i].a.pos.y << ' ' << tris[i].a.pos.z << '\n';
        fout << tris[i].b.pos.x << ' ' << tris[i].b.pos.y << ' ' << tris[i].b.pos.z << '\n';
        fout << tris[i].c.pos.x << ' ' << tris[i].c.pos.y << ' ' << tris[i].c.pos.z << '\n';
        fout << colors[i]/256/256 << ' ' << colors[i]/256%256 << ' ' << colors[i]%256 << '\n';
        fout << tris[i].mirror << ' ' << tris[i].diffuse << '\n';
    }
    n = lights.size();
    fout << n;
    for (int i=0;i<n;i++){
        fout << lights[i].x << ' ' << lights[i].y << ' ' << lights[i].z << '\n';
        fout << lcolors[i]/256/256 << ' ' << lcolors[i]/256%256 << ' ' << lcolors[i]%256 << '\n';
        fout << lintensity[i] << '\n';
    }
    fout.close();
    return true;
}


// RTX


Vec3 GenerateCameraRay(float x, float y, Vec3& rayDir, Vec3 &cF, Vec3 &cR, Vec3 &cU, float tanFovX, float tanFovY)
{
    float ndcX = (x + 0.5f) / WIDTH;
    float ndcY = (y + 0.5f) / HEIGHT;

    float sx = (2.0f * ndcX - 1.0f) * tanFovX;
    float sy = (1.0f - 2.0f * ndcY) * tanFovY;

    rayDir.x = cF.x + cR.x * sx - cU.x * sy;
    rayDir.y = cF.y + cR.y * sx - cU.y * sy;
    rayDir.z = cF.z + cR.z * sx - cU.z * sy;

    return rayDir;
}

bool IntersectTriangle(const Ray& ray,
                              const triangle& tri,
                              float& outT,
                              float& outU,
                              float& outV)
{
    const float EPS = 1e-6f;

    float e1x = tri.edgeA.x;
    float e1y = tri.edgeA.y;
    float e1z = tri.edgeA.z;

    float e2x = tri.edgeB.x;
    float e2y = tri.edgeB.y;
    float e2z = tri.edgeB.z;

    float hx = ray.dir.y * e2z - ray.dir.z * e2y;
    float hy = ray.dir.z * e2x - ray.dir.x * e2z;
    float hz = ray.dir.x * e2y - ray.dir.y * e2x;

    float a = e1x * hx + e1y * hy + e1z * hz;
    if (a > -EPS && a < EPS) return false;

    float f = 1.0f / a;

    float sx = ray.origin.x - tri.a.pos.x;
    float sy = ray.origin.y - tri.a.pos.y;
    float sz = ray.origin.z - tri.a.pos.z;

    float u = f * (sx * hx + sy * hy + sz * hz);
    if (u < 0.0f || u > 1.0f) return false;

    float qx = sy * e1z - sz * e1y;
    float qy = sz * e1x - sx * e1z;
    float qz = sx * e1y - sy * e1x;

    float v = f * (ray.dir.x * qx + ray.dir.y * qy + ray.dir.z * qz);
    if (v < 0.0f || u + v > 1.0f) return false;

    float t = f * (e2x * qx + e2y * qy + e2z * qz);
    if (t > EPS)
    {
        outT = t;
        outU = u;
        outV = v;
        return true;
    }

    return false;
}

float TraceLightRay(Vec3 hitPoint, Vec3 &light, float lightDist){

    Vec3 toLight;
    toLight.x = light.x - hitPoint.x;
    toLight.y = light.y - hitPoint.y;
    toLight.z = light.z - hitPoint.z;

    float invLen = 1.0f / lightDist;
    toLight.x *= invLen;
    toLight.y *= invLen;
    toLight.z *= invLen;

    Ray shadowRay = { hitPoint, toLight };

    float howshaded = 1.0f;

    for (int i = 0; i < tris.size(); i++)
    {
        float t, u, v;
        if (IntersectTriangle(shadowRay, tris[i], t, u, v))
        {
            if (t < lightDist)
            {
                howshaded -= (1-tris[i].transparency);
            }
        }
    }
    return std::max(howshaded, 0.0f);
}

inline float randf()
{
    return rand() / (float)RAND_MAX;
}
Vec3 RandomInUnitSphere()
{
    while (true)
    {
        float x = randf() * 2.0f - 1.0f;
        float y = randf() * 2.0f - 1.0f;
        float z = randf() * 2.0f - 1.0f;

        float len2 = x*x + y*y + z*z;

        if (len2 <= 1.0f)
            return { x, y, z };
    }
}

Vec3 RandomInHemisphere(const Vec3& normal)
{
    Vec3 r = RandomInUnitSphere();

    if (dot(r, normal) < 0.0f)
        r = -r;

    return normalize(r);
}

Color Shade(const Vec3& hitPoint,
            const Vec3& normal,
            uint32_t col)
{
    Color baseColor;
    baseColor.r = ((col >> 16) & 0xFF) / 255.0f;
    baseColor.g = ((col >> 8) & 0xFF) / 255.0f;
    baseColor.b = ((col) & 0xFF) / 255.0f;

    Color result = {0,0,0};

    for (int j = 0; j < lights.size(); j++)
    {
        Vec3 L = lights[j] - hitPoint;
        float dist2 = dot(L, L);
        float dist  = sqrtf(dist2);

        Vec3 lightDir = L / dist;

        float NdotL = dot(normal, lightDir);
        if (NdotL <= 0.0f)
            continue;

        Vec3 origin = hitPoint + normal * 0.001f;

        float visibility = 0.0f;
        int samples = 16;

        for (int i = 0; i < samples; i++)
        {
            Vec3 offset = RandomInUnitSphere() * 0.1f;
            Vec3 lightSample = lights[j] + offset;

            visibility += TraceLightRay(origin, lightSample, dist);
        }

        visibility /= samples;


        float attenuation = 1.0f / dist;

        Color lightColor;
        lightColor.r = ((lcolors[j] >> 16) & 0xFF) / 255.0f;
        lightColor.g = ((lcolors[j] >> 8) & 0xFF) / 255.0f;
        lightColor.b = ((lcolors[j]) & 0xFF) / 255.0f;

        float diffuse = NdotL;

        result.r += baseColor.r * lightColor.r * diffuse * attenuation * visibility * lintensity[j];
        result.g += baseColor.g * lightColor.g * diffuse * attenuation * visibility * lintensity[j];
        result.b += baseColor.b * lightColor.b * diffuse * attenuation * visibility * lintensity[j];
    }
    return result;
}
Color TraceRay(const Ray& ray, int depth)
{
    if (depth > 7)
        return {0,0,0};

    float closestT = 99999999;
    const triangle* hitTri = nullptr;
    uint32_t tcolor = 0xFFFFFFFF;

    for (size_t i=0;i<tris.size();i++)
    {
        triangle tr = tris[i];
        float t, u, v;
        if (IntersectTriangle(ray, tr, t, u, v))
        {
            if (t < closestT)
            {
                closestT = t;
                hitTri = &tris[i];
                tcolor = colors[i];
            }
        }
    }

    Color albedo = {
        float((tcolor & 0x00FF0000) >> 16) / 255.0f,
        float((tcolor & 0x0000FF00) >> 8) / 255.0f,
        float(tcolor & 0x000000FF) / 255.0f
    };

    if (!hitTri)
        return {0,0,0};

    Vec3 hitPoint = ray.origin + ray.dir * closestT;
    Vec3 normal = hitTri->normal;

    if (dot(normal, ray.dir) > 0)
    normal = -normal;

    Color result = Shade(hitPoint, normal, tcolor);

    // THE RUSSIAN ROULETTE
    float survivalProbability = 1.0f;

    if (depth > 6)
    {
        survivalProbability = std::max(hitTri->mirror, std::max(hitTri->diffuse, hitTri->transparency));
        survivalProbability = clamp(survivalProbability, 0.1f, 0.9f);

        if (randf() > survivalProbability)
            return result;  // the ray dies
    }

    bool rouletting = 0;

    float choice = rand()%256/256*(hitTri->mirror+hitTri->diffuse+hitTri->transparency);


    // mirroring it
    if (choice < hitTri->mirror)
    {
        rouletting = 1;
        Vec3 R = ray.dir - normal*(2.0f * dot(ray.dir, normal));
        Ray reflectRay = { hitPoint + normal * 0.001f, R };

        Color reflected = TraceRay(reflectRay, depth + 1);

        result = result + reflected * albedo * hitTri->mirror * survivalProbability;
    }
    else
    // diffusing it (I don't know what's wrong with this)
    if (choice >= hitTri->mirror && choice < hitTri->mirror + hitTri->diffuse)
    {
        rouletting = 1;
        Vec3 bounceDir = RandomInHemisphere(normal);
        Ray bounceRay = { hitPoint + normal * 0.001f, bounceDir };

        Color bounced = TraceRay(bounceRay, depth + 1);

        float cosTerm = std::max(0.0f, dot(normal, bounceDir));

        result = result + bounced * cosTerm * albedo * hitTri->diffuse * survivalProbability;
    }
    else
    if (choice >= hitTri->mirror + hitTri->diffuse && choice < hitTri->mirror + hitTri->diffuse + hitTri->transparency)
    {
        rouletting = 1;
        Ray refractedRay = { hitPoint + ray.dir * 0.001f, ray.dir };
        Color transmitted = TraceRay(refractedRay, depth + 1);

        result = result * (1.0f - hitTri->transparency) + transmitted * hitTri->transparency;
    }

    if (rouletting)
        return result / survivalProbability;
    else
        return result;
}

int PointAtRay(const Ray& ray)
{
    float closestT = 99999999;
    int hitTri = -1;

    for (size_t i=0;i<tris.size();i++)
    {
        triangle tr = tris[i];
        float t, u, v;
        if (IntersectTriangle(ray, tr, t, u, v))
        {
            if (t < closestT)
            {
                closestT = t;
                hitTri = i;
            }
        }
    }
    return hitTri;
}

uint32_t ToRGBA(const Color& c)
{
    float r = clamp(c.r, 0.0f, 1.0f);
    float g = clamp(c.g, 0.0f, 1.0f);
    float b = clamp(c.b, 0.0f, 1.0f);

    uint8_t R = (uint8_t)(r * 255.0f);
    uint8_t G = (uint8_t)(g * 255.0f);
    uint8_t B = (uint8_t)(b * 255.0f);

    return (R << 16) | (G << 8) | B;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "GDITriangle";
    RegisterClass(&wc);

    HWND hwnd = CreateWindow(
        wc.lpszClassName, "OH HELL YEAH",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100, WIDTH, HEIGHT,
        nullptr, nullptr, hInst, nullptr
    );

    int n;

    std::cout << "Input the .obj file name: ";
    std::string file = "";
    std::cin >> file;
    LoadOBJTriangles(file,verts, tris);
    for (int i=0;i<tris.size();i++){
        //colors.push_back((rand()%256)*256*256 + (rand()%256)*256 + (rand()%256));
        colors.push_back(0x00FFFFFF);
        tris[i].mirror = 0.3f;
        tris[i].diffuse = 0.0f;
    }
    n = tris.size();

    Camera camera;
    camera.pos   = { 0, 0, 0 };
    camera.yaw   = 0.0f;
    camera.pitch = 0.0f;

    Vec3 lightDir = normalize({ 0.3f, 0.8f, -1.0f });


    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = WIDTH;
    bmi.bmiHeader.biHeight = -HEIGHT;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(hwnd);
    dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&framebuffer, nullptr, 0);

    memDC = CreateCompatibleDC(hdc);
    SelectObject(memDC, dib);
    ReleaseDC(hwnd, hdc);

    MSG msg = {};
    HDC winDC = GetDC(hwnd);
    POINT pcd, cd;
    GetCursorPos(&pcd);

    float speed = 1.0f;
    InitTimer();
    int lpick = 0;

    float pdt = 0.0f;

    SHORT deletebtn = 0;

    while (msg.message != WM_QUIT)
    {
        float dt = GetDeltaTime();
        if (1000/(dt*1000.0f) > 100) dt = pdt;
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }



        GetCursorPos(&cd);
        if (cd.x != pcd.x) camera.yaw += (cd.x-pcd.x)*0.01f;
        if (cd.y != pcd.y) camera.pitch += (cd.y-pcd.y)*0.01f;
        NormalizeCamera(camera);

        if (GetKeyState('W') & 0x8000)
        MoveCamera(camera, 1,  0, 0.01, speed);

        if (GetKeyState('S') & 0x8000)
        MoveCamera(camera, -1,  0, 0.01, speed);

        if (GetKeyState('A') & 0x8000)
        MoveCamera(camera, 0, -1, 0.01, speed);

        if (GetKeyState('D') & 0x8000)
        MoveCamera(camera, 0, 1, 0.01, speed);


        Clear(0x00000000);
        ClearZ();

        int total = n;
        minZ = 99999999;
        maxZ = 0;

        Vec3 viewlightDir = ViewSpaceDir(lightDir, camera);

        tempverts.resize(0);

        if (GetKeyState(VK_DOWN) < 0){
            Vec3 lpos = {camera.pos.x, camera.pos.y, camera.pos.z};
            int a = lpick%3;
            int r, g, b; float intens = 1.0f;
            std::cin >> r >> g >> b;
            std::cin >> intens;
            uint32_t lcolor = r*256*256+g*256+b;
            lights.push_back(lpos);
            lcolors.push_back(lcolor);
            lintensity.push_back(intens);
            dt = 0.01;
        }

        if (GetKeyState(0x43) < 0){
            float cubesize, mirr, diff, trans;
            int r,g,b;
            std::cin >> cubesize >> r >> g >> b >> mirr >> diff >> trans;
            for (int i=0;i<sampleCube.size();i++){
                triangle tri;
                tri.a.pos = (sampleCube[i].a-0.5f)*cubesize+camera.pos;
                tri.b.pos = (sampleCube[i].b-0.5f)*cubesize+camera.pos;
                tri.c.pos = (sampleCube[i].c-0.5f)*cubesize+camera.pos;
                tri.normal = normalize(cross(tri.b.pos - tri.a.pos, tri.c.pos - tri.a.pos));
                tri.a.normal = tri.normal;
                tri.b.normal = tri.normal;
                tri.c.normal = tri.normal;
                tri.edgeA = tri.b.pos - tri.a.pos;
                tri.edgeB = tri.c.pos - tri.a.pos;
                tri.mirror = mirr;
                tri.diffuse = diff;
                tri.transparency = trans;
                tris.push_back(tri);
                colors.push_back(r*256*256+g*256+b);
                printf("%d\n",tris.size());
                printf("%d\n",colors.size());
            }
            dt = 0.01;
        }

        if (GetKeyState(VK_RIGHT) < 0){
            std::string file = "";
            std::cin >> file;
            WriteToXSCENE(file);
            dt = 0.01;
        }

        Camera cam2 = {camera.pos, camera.yaw, -camera.pitch};
        Vec3 cf = GetForward(cam2);
        Ray ray = {cam2.pos, cf};
        int pointed = PointAtRay(ray);

        if (GetKeyState(VK_BACK) < 0 && deletebtn >= 0 && pointed != -1){
            tris.erase(tris.begin()+pointed);
            colors.erase(colors.begin()+pointed);
        }
        deletebtn = GetKeyState(VK_BACK);

        if (GetKeyState(VK_UP) < 0){
            Vec3 cr = GetRight(cam2);
            Vec3 cu = GetUp(cam2);

            float tanFovY = tanf(fov/2.0f);
            float tanFovX = tanFovY * aspect;

            for (float y = 0; y < HEIGHT; y++){
                for (float x = 0; x < WIDTH; x++){
                    Color total = {0,0,0};
                    for (int i=0;i<16;i++){
                        Vec3 dir;
                        GenerateCameraRay(x+float(rand()%4)/4.0f, y+(rand()%4)/4.0f, dir, cf, cr, cu, tanFovX, tanFovY);
                        Ray ray = {cam2.pos, dir};
                        total = total + TraceRay(ray, 0);
                    }
                    total = total/16.0f;
                    total.r = pow(total.r, 1.0f/2.2);
                    total.g = pow(total.g, 1.0f/2.2);
                    total.b = pow(total.b, 1.0f/2.2);
                    framebuffer[int(x) + int(y) * WIDTH] = ToRGBA(total);
                }
                if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                    continue;
                }
                StretchBlt(winDC, 0, 0, 1920, 1080, memDC, 0, 0, WIDTH, HEIGHT, SRCCOPY);
            }
            Sleep(3000);
            dt = 0.01;
        }
        else{
            for (int i=0;i<tris.size();i++){

                triangle ctri = {
                    {ViewSpace(tris[i].a.pos, camera), ViewSpaceDir(tris[i].a.normal, camera)},
                    {ViewSpace(tris[i].b.pos, camera), ViewSpaceDir(tris[i].b.normal, camera)},
                    {ViewSpace(tris[i].c.pos, camera), ViewSpaceDir(tris[i].c.normal, camera)}};

                std::vector<triangle> clipped;
                ClipTriangleNearPlane(ctri, 0.1, clipped);


                for (auto& t : clipped) {

                    Vec3 n = TriangleNormal({t.a.pos, t.b.pos, t.c.pos});

                    Vec3 p0 = TransformAll(t.a.pos);
                    Vec3 p1 = TransformAll(t.b.pos);
                    Vec3 p2 = TransformAll(t.c.pos);

                    if (i != pointed)
                    DrawTriangle(
                        p0.x, p0.y, 1.0f - p0.z,
                        p1.x, p1.y, 1.0f - p1.z,
                        p2.x, p2.y, 1.0f - p2.z,
                        t.a.normal, t.b.normal, t.c.normal, viewlightDir,
                        colors[i]
                    );
                    else{
                        DrawTriangle(
                        p0.x, p0.y, 1.0f - p0.z,
                        p1.x, p1.y, 1.0f - p1.z,
                        p2.x, p2.y, 1.0f - p2.z,
                        t.a.normal, t.b.normal, t.c.normal, viewlightDir,
                        0x00FFFFFF
                        );
                    }
                }
            }
        }

        //printf("%.2f, %d, %.2f, %.2f\n", 1000/(dt*1000.0f), total, minZ, maxZ);


        StretchBlt(winDC, 0, 0, 1920, 1080, memDC, 0, 0, WIDTH, HEIGHT, SRCCOPY);
        pcd = cd;
        pdt = dt;
    }
    ReleaseDC(hwnd, winDC);
    return 0;
}
