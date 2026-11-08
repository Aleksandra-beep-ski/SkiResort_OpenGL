#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>

const float PI = 3.14159265358979323846f;
const int   SCR_W = 1280;
const int   SCR_H = 720;
const int   SHADOW_W = 2048;
const int   SHADOW_H = 2048;
const int   NUM_SNOW = 3000;

const float TRACK_MIN_X = -4.0f;
const float TRACK_MAX_X = 4.0f;

float heightAt(float x, float z) {
    return 0.12f * z + 0.15f * std::sin(x * 0.6f) * std::cos(z * 0.4f);
}

const char* MAIN_VS =
"#version 330 core\n"
"layout(location = 0) in vec3 aPos;\n"
"layout(location = 1) in vec3 aNormal;\n"
"layout(location = 2) in vec2 aTexCoord;\n"
"uniform mat4 model;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"uniform mat4 lightSpaceMatrix;\n"
"out vec3 FragPos;\n"
"out vec3 Normal;\n"
"out vec2 TexCoord;\n"
"out vec4 FragPosLightSpace;\n"
"void main() {\n"
"    vec4 wp = model * vec4(aPos, 1.0);\n"
"    FragPos = wp.xyz;\n"
"    Normal = mat3(transpose(inverse(model))) * aNormal;\n"
"    TexCoord = aTexCoord;\n"
"    FragPosLightSpace = lightSpaceMatrix * wp;\n"
"    gl_Position = projection * view * wp;\n"
"}\n";

const char* MAIN_FS =
"#version 330 core\n"
"in vec3 FragPos;\n"
"in vec3 Normal;\n"
"in vec2 TexCoord;\n"
"in vec4 FragPosLightSpace;\n"
"out vec4 FragColor;\n"
"uniform sampler2D diffuseTex;\n"
"uniform sampler2D shadowMap;\n"
"uniform vec3 viewPos;\n"
"uniform vec3 dirLightDir;\n"
"uniform vec3 dirLightAmbient;\n"
"uniform vec3 dirLightDiffuse;\n"
"uniform vec3 dirLightSpecular;\n"
"uniform vec3 pointLightPos;\n"
"uniform vec3 pointLightAmbient;\n"
"uniform vec3 pointLightDiffuse;\n"
"uniform vec3 pointLightSpecular;\n"
"uniform float pointLightConst;\n"
"uniform float pointLightLinear;\n"
"uniform float pointLightQuad;\n"
"uniform vec3 matAmbient;\n"
"uniform vec3 matDiffuse;\n"
"uniform vec3 matSpecular;\n"
"uniform float matShininess;\n"
"uniform vec3 fogColor;\n"
"uniform float fogStart;\n"
"uniform float fogEnd;\n"
"float shadowCalc(vec3 N, vec3 L) {\n"
"    vec3 proj = FragPosLightSpace.xyz / FragPosLightSpace.w;\n"
"    proj = proj * 0.5 + 0.5;\n"
"    if (proj.z > 1.0) return 0.0;\n"
"    float bias = max(0.005 * (1.0 - dot(N, L)), 0.0005);\n"
"    float sh = 0.0;\n"
"    vec2 texel = 1.0 / textureSize(shadowMap, 0);\n"
"    for (int x = -1; x <= 1; x++)\n"
"        for (int y = -1; y <= 1; y++) {\n"
"            float d = texture(shadowMap, proj.xy + vec2(x, y) * texel).r;\n"
"            sh += (proj.z - bias > d) ? 1.0 : 0.0;\n"
"        }\n"
"    return sh / 9.0;\n"
"}\n"
"vec3 calcDirLight(vec3 N, vec3 V) {\n"
"    vec3 L = normalize(-dirLightDir);\n"
"    vec3 tex = texture(diffuseTex, TexCoord).rgb;\n"
"    vec3 amb = dirLightAmbient * matAmbient * tex;\n"
"    float d = max(dot(N, L), 0.0);\n"
"    vec3 dif = dirLightDiffuse * d * matDiffuse * tex;\n"
"    vec3 H = normalize(L + V);\n"
"    float s = pow(max(dot(N, H), 0.0), matShininess);\n"
"    vec3 spec = dirLightSpecular * s * matSpecular;\n"
"    float sh = shadowCalc(N, L);\n"
"    return amb + (1.0 - sh) * (dif + spec);\n"
"}\n"
"vec3 calcPointLight(vec3 N, vec3 V) {\n"
"    vec3 L = normalize(pointLightPos - FragPos);\n"
"    vec3 tex = texture(diffuseTex, TexCoord).rgb;\n"
"    vec3 amb = pointLightAmbient * matAmbient * tex;\n"
"    float d = max(dot(N, L), 0.0);\n"
"    vec3 dif = pointLightDiffuse * d * matDiffuse * tex;\n"
"    vec3 H = normalize(L + V);\n"
"    float s = pow(max(dot(N, H), 0.0), matShininess);\n"
"    vec3 spec = pointLightSpecular * s * matSpecular;\n"
"    float dist = length(pointLightPos - FragPos);\n"
"    float att = 1.0 / (pointLightConst + pointLightLinear * dist + pointLightQuad * dist * dist);\n"
"    return (amb + dif + spec) * att;\n"
"}\n"
"void main() {\n"
"    vec3 N = normalize(Normal);\n"
"    vec3 V = normalize(viewPos - FragPos);\n"
"    vec3 color = calcDirLight(N, V) + calcPointLight(N, V);\n"
"    float distance = length(viewPos - FragPos);\n"
"    float fogFactor = smoothstep(fogStart, fogEnd, distance);\n"
"    color = mix(color, fogColor, fogFactor);\n"
"    FragColor = vec4(color, 1.0);\n"
"}\n";

const char* SHADOW_VS =
"#version 330 core\n"
"layout(location = 0) in vec3 aPos;\n"
"uniform mat4 model;\n"
"uniform mat4 lightSpaceMatrix;\n"
"void main() {\n"
"    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);\n"
"}\n";

const char* SHADOW_FS =
"#version 330 core\n"
"void main() {}\n";

const char* SNOW_VS =
"#version 330 core\n"
"layout(location = 0) in vec3 aPos;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"uniform float pointSize;\n"
"void main() {\n"
"    vec4 viewPos = view * vec4(aPos, 1.0);\n"
"    gl_Position = projection * viewPos;\n"
"    float dist = length(viewPos.xyz);\n"
"    gl_PointSize = clamp(pointSize * 15.0 / max(dist, 0.1), 1.0, 25.0);\n"
"}\n";

const char* SNOW_FS =
"#version 330 core\n"
"out vec4 FragColor;\n"
"void main() {\n"
"    vec2 c = gl_PointCoord - vec2(0.5);\n"
"    if (dot(c, c) > 0.25) discard;\n"
"    FragColor = vec4(1.0, 1.0, 1.0, 0.85);\n"
"}\n";

class Shader {
public:
    unsigned int ID = 0;
    Shader() {}
    Shader(const char* vs, const char* fs) {
        unsigned int v = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(v, 1, &vs, NULL);
        glCompileShader(v);
        unsigned int f = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(f, 1, &fs, NULL);
        glCompileShader(f);
        ID = glCreateProgram();
        glAttachShader(ID, v); glAttachShader(ID, f);
        glLinkProgram(ID);
        glDeleteShader(v); glDeleteShader(f);
    }
    void use() const { glUseProgram(ID); }
    void setInt(const std::string& n, int x) const { glUniform1i(glGetUniformLocation(ID, n.c_str()), x); }
    void setFloat(const std::string& n, float x) const { glUniform1f(glGetUniformLocation(ID, n.c_str()), x); }
    void setVec3(const std::string& n, const glm::vec3& x) const { glUniform3fv(glGetUniformLocation(ID, n.c_str()), 1, glm::value_ptr(x)); }
    void setMat4(const std::string& n, const glm::mat4& x) const { glUniformMatrix4fv(glGetUniformLocation(ID, n.c_str()), 1, GL_FALSE, glm::value_ptr(x)); }
};

struct Mesh { GLuint VAO = 0, VBO = 0, EBO = 0; int indexCount = 0; };

Mesh uploadMesh(const std::vector<float>& v, const std::vector<unsigned int>& idx) {
    Mesh m;
    glGenVertexArrays(1, &m.VAO); glGenBuffers(1, &m.VBO); glGenBuffers(1, &m.EBO);
    glBindVertexArray(m.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m.VBO); glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.EBO); glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);
    m.indexCount = (int)idx.size();
    return m;
}

Mesh createCube() {
    float s = 0.5f; std::vector<float> v; std::vector<unsigned int> idx;
    v.insert(v.end(), { -s,-s, s, 0,0,1, 0,0,  s,-s, s, 0,0,1, 1,0,  s, s, s, 0,0,1, 1,1, -s, s, s, 0,0,1, 0,1 });
    v.insert(v.end(), { s,-s,-s, 0,0,-1, 0,0, -s,-s,-s, 0,0,-1, 1,0, -s, s,-s, 0,0,-1, 1,1,  s, s,-s, 0,0,-1, 0,1 });
    v.insert(v.end(), { s,-s, s, 1,0,0, 0,0,  s,-s,-s, 1,0,0, 1,0,  s, s,-s, 1,0,0, 1,1,  s, s, s, 1,0,0, 0,1 });
    v.insert(v.end(), { -s,-s,-s,-1,0,0, 0,0, -s,-s, s,-1,0,0, 1,0, -s, s, s,-1,0,0, 1,1, -s, s,-s,-1,0,0, 0,1 });
    v.insert(v.end(), { -s, s, s, 0,1,0, 0,0,  s, s, s, 0,1,0, 1,0,  s, s,-s, 0,1,0, 1,1, -s, s,-s, 0,1,0, 0,1 });
    v.insert(v.end(), { -s,-s,-s, 0,-1,0, 0,0,  s,-s,-s, 0,-1,0, 1,0,  s,-s, s, 0,-1,0, 1,1, -s,-s, s, 0,-1,0, 0,1 });
    for (int f = 0; f < 6; f++) { int b = f * 4; idx.insert(idx.end(), { (unsigned)b, (unsigned)(b + 1), (unsigned)(b + 2), (unsigned)b, (unsigned)(b + 2), (unsigned)(b + 3) }); }
    return uploadMesh(v, idx);
}

Mesh createCylinder(int N) {
    std::vector<float> v; std::vector<unsigned int> idx; float hh = 0.5f, r = 1.0f;
    for (int i = 0; i <= N; i++) {
        float a = 2 * PI * i / N, c = std::cos(a), s = std::sin(a);
        v.insert(v.end(), { r * c, -hh, r * s, c,0,s, (float)i / N, 0 });
        v.insert(v.end(), { r * c,  hh, r * s, c,0,s, (float)i / N, 1 });
    }
    for (int i = 0; i < N; i++) { int b = i * 2; idx.insert(idx.end(), { (unsigned)b, (unsigned)(b + 1), (unsigned)(b + 2), (unsigned)(b + 2), (unsigned)(b + 1), (unsigned)(b + 3) }); }
    int tc = v.size() / 8; v.insert(v.end(), { 0, hh, 0, 0,1,0, 0.5f, 0.5f }); int ts = v.size() / 8;
    for (int i = 0; i <= N; i++) { float a = 2 * PI * i / N, c = std::cos(a), s = std::sin(a); v.insert(v.end(), { r * c, hh, r * s, 0,1,0, 0.5f + 0.5f * c, 0.5f + 0.5f * s }); }
    for (int i = 0; i < N; i++) idx.insert(idx.end(), { (unsigned)tc, (unsigned)(ts + i), (unsigned)(ts + i + 1) });
    int bc = v.size() / 8; v.insert(v.end(), { 0, -hh, 0, 0,-1,0, 0.5f, 0.5f }); int bs = v.size() / 8;
    for (int i = 0; i <= N; i++) { float a = 2 * PI * i / N, c = std::cos(a), s = std::sin(a); v.insert(v.end(), { r * c, -hh, r * s, 0,-1,0, 0.5f + 0.5f * c, 0.5f + 0.5f * s }); }
    for (int i = 0; i < N; i++) idx.insert(idx.end(), { (unsigned)bc, (unsigned)(bs + i + 1), (unsigned)(bs + i) });
    return uploadMesh(v, idx);
}

Mesh createCone(int N) {
    std::vector<float> v; std::vector<unsigned int> idx; float r = 1.0f, h = 1.0f;
    for (int i = 0; i < N; i++) {
        float a0 = 2 * PI * i / N, a1 = 2 * PI * (i + 1) / N;
        float c0 = std::cos(a0), s0 = std::sin(a0), c1 = std::cos(a1), s1 = std::sin(a1);
        glm::vec3 n0 = glm::normalize(glm::vec3(c0 * h, r, s0 * h)), n1 = glm::normalize(glm::vec3(c1 * h, r, s1 * h)), na = glm::normalize(n0 + n1);
        int b = v.size() / 8;
        v.insert(v.end(), { r * c0, 0, r * s0, n0.x, n0.y, n0.z, (float)i / N, 0 });
        v.insert(v.end(), { r * c1, 0, r * s1, n1.x, n1.y, n1.z, (float)(i + 1) / N, 0 });
        v.insert(v.end(), { 0, h, 0, na.x, na.y, na.z, (float)(i + 0.5f) / N, 1 });
        idx.insert(idx.end(), { (unsigned)b, (unsigned)(b + 1), (unsigned)(b + 2) });
    }
    int bc = v.size() / 8; v.insert(v.end(), { 0,0,0, 0,-1,0, 0.5f,0.5f }); int bs = v.size() / 8;
    for (int i = 0; i <= N; i++) { float a = 2 * PI * i / N, c = std::cos(a), s = std::sin(a); v.insert(v.end(), { r * c, 0, r * s, 0,-1,0, 0.5f + 0.5f * c, 0.5f + 0.5f * s }); }
    for (int i = 0; i < N; i++) idx.insert(idx.end(), { (unsigned)bc, (unsigned)(bs + i + 1), (unsigned)(bs + i) });
    return uploadMesh(v, idx);
}

Mesh createTerrain(float size, int N) {
    std::vector<float> v; std::vector<unsigned int> idx; float half = size * 0.5f;
    for (int i = 0; i <= N; i++) for (int j = 0; j <= N; j++) {
        float x = -half + size * i / N, z = -half + size * j / N, y = heightAt(x, z), eps = size / N * 0.5f;
        float hL = heightAt(x - eps, z), hR = heightAt(x + eps, z), hD = heightAt(x, z - eps), hU = heightAt(x, z + eps);
        glm::vec3 n = glm::normalize(glm::vec3(hL - hR, 2 * eps, hD - hU));
        v.insert(v.end(), { x, y, z, n.x, n.y, n.z, x * 0.3f, z * 0.3f });
    }
    for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) {
        int a = i * (N + 1) + j, b = a + 1, c = a + (N + 1), d = c + 1;
        idx.insert(idx.end(), { (unsigned)a, (unsigned)c, (unsigned)b, (unsigned)b, (unsigned)c, (unsigned)d });
    }
    return uploadMesh(v, idx);
}

float hash01(int ix, int iy, int seed) {
    unsigned int h = (unsigned int)ix * 374761393u + (unsigned int)iy * 668265263u + (unsigned int)seed * 2654435761u;
    h = (h ^ (h >> 13)) * 1274126177u; h = h ^ (h >> 16);
    return (h & 0xffffff) / (float)0xffffff;
}
float valueNoise(float x, float y, int seed) {
    int ix = (int)std::floor(x), iy = (int)std::floor(y); float fx = x - ix, fy = y - iy;
    float a = hash01(ix, iy, seed), b = hash01(ix + 1, iy, seed), c = hash01(ix, iy + 1, seed), d = hash01(ix + 1, iy + 1, seed);
    float u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
    return glm::mix(glm::mix(a, b, u), glm::mix(c, d, u), v);
}
float fbm(float x, float y, int seed, int oct = 4) {
    float sum = 0, amp = 0.5f, f = 1.0f;
    for (int i = 0; i < oct; i++) { sum += amp * valueNoise(x * f, y * f, seed + i); f *= 2; amp *= 0.5f; }
    return sum;
}
GLuint uploadTexture(int w, int h, const unsigned char* data) {
    GLuint tex; glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return tex;
}
template <typename F> GLuint makeTexture(int size, F fn) {
    std::vector<unsigned char> d(size * size * 4);
    for (int y = 0; y < size; y++) for (int x = 0; x < size; x++) {
        glm::vec3 c = glm::clamp(fn(x, y), glm::vec3(0.0f), glm::vec3(1.0f));
        int i = (y * size + x) * 4;
        d[i + 0] = (unsigned char)(c.r * 255); d[i + 1] = (unsigned char)(c.g * 255); d[i + 2] = (unsigned char)(c.b * 255); d[i + 3] = 255;
    }
    return uploadTexture(size, size, d.data());
}

struct Piece { Mesh* mesh; GLuint tex; glm::mat4 local; glm::vec3 mAmb, mDif, mSpec; float shin; };
Mesh gCube, gCylinder, gCone, gPyramid, gTerrain;
std::vector<Piece> staticPieces, skierPieces, cabinPieces;

glm::vec3 camTarget(0.0f, 0.0f, 0.0f);
float camYaw = 45.0f, camPitch = 30.0f, camDistance = 20.0f;
bool mouseDown = false, firstMouse = true; double prevMouseX = 0, prevMouseY = 0;

float animTime = 0.0f, skierDistance = 0.0f, skierLateral = 0.0f, skierLateralSpeed = 0.0f, speedMul = 0.0f; bool paused = false;

struct SnowParticle { glm::vec3 pos; float fallSpeed, windX, windZ; };
std::vector<SnowParticle> snowParticles;
GLuint snowVAO = 0, snowVBO = 0;
glm::vec3 cableFrom, cableTo;

glm::vec3 getCameraPos() {
    float ry = glm::radians(camYaw), rp = glm::radians(camPitch);
    return camTarget + camDistance * glm::vec3(std::cos(rp) * std::sin(ry), std::sin(rp), std::cos(rp) * std::cos(ry));
}
float getSkierSpeed() { return 3.0f * (1.0f + speedMul); }

glm::mat4 getSkierTransform() {
    float speed = getSkierSpeed();
    float z = 14.0f - std::fmod(skierDistance, 28.0f);
    float phase = skierDistance * 0.27f;

    float x = skierLateral + 1.5f * std::sin(phase);
    x = glm::clamp(x, TRACK_MIN_X, TRACK_MAX_X);

    float y = heightAt(x, z) + 0.05f;

    float dxdt = 1.5f * 0.27f * speed * std::cos(phase) + skierLateralSpeed;
    float dzdt = -speed;
    float yaw = std::atan2(-dxdt, -dzdt);

    return glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z)) * glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0, 1, 0));
}

glm::mat4 getCabinTransform(int i) {
    float u = 0.1f + 0.8f * (0.5f + 0.5f * std::sin(animTime * 0.3f + i * PI));
    glm::vec3 pos = glm::mix(cableFrom, cableTo, u);

    if (i == 0) pos.x -= 1.0f;
    else pos.x += 1.0f;

    return glm::translate(glm::mat4(1.0f), pos);
}

void updateSnow(float dt) {
    for (auto& p : snowParticles) {
        p.pos.y -= p.fallSpeed * dt;
        p.pos.x += p.windX * dt; p.pos.z += p.windZ * dt;

        if (p.pos.y < heightAt(p.pos.x, p.pos.z) - 0.5f) {
            p.pos = glm::vec3((rand() % 10000) / 100.0f - 50.0f, 30.0f + (rand() % 1000) / 100.0f, (rand() % 10000) / 100.0f - 50.0f);
        }
    }
}
void updateSnowVBO() {
    std::vector<glm::vec3> pos(NUM_SNOW);
    for (int i = 0; i < NUM_SNOW; i++) pos[i] = snowParticles[i].pos;
    glBindBuffer(GL_ARRAY_BUFFER, snowVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec3) * NUM_SNOW, pos.data());
}

void framebuffer_size_callback(GLFWwindow*, int w, int h) { glViewport(0, 0, w, h); }
void mouse_button_callback(GLFWwindow*, int btn, int action, int) { if (btn == GLFW_MOUSE_BUTTON_LEFT) mouseDown = (action == GLFW_PRESS); }
void cursor_callback(GLFWwindow*, double x, double y) {
    if (firstMouse) { prevMouseX = x; prevMouseY = y; firstMouse = false; return; }
    if (!mouseDown) { prevMouseX = x; prevMouseY = y; return; }
    double dx = x - prevMouseX, dy = y - prevMouseY; prevMouseX = x; prevMouseY = y;
    camYaw -= (float)dx * 0.3f; camPitch += (float)dy * 0.3f;
    camPitch = glm::clamp(camPitch, -89.0f, 89.0f);
}
void scroll_callback(GLFWwindow*, double, double yoff) { camDistance -= (float)yoff * 1.5f; camDistance = glm::clamp(camDistance, 3.0f, 150.0f); }
void key_callback(GLFWwindow* win, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(win, true);
    if (key == GLFW_KEY_SPACE)  paused = !paused;
    if (key == GLFW_KEY_R) { animTime = 0; skierDistance = 0; skierLateral = 0; skierLateralSpeed = 0; speedMul = 0; camTarget = glm::vec3(0); camYaw = 45; camPitch = 30; camDistance = 20; }
}

void processInput(GLFWwindow* win, float dt) {
    glm::vec3 fwd(-std::sin(glm::radians(camYaw)), 0, -std::cos(glm::radians(camYaw)));
    glm::vec3 rgt(std::cos(glm::radians(camYaw)), 0, -std::sin(glm::radians(camYaw)));
    float camSpeed = 10.0f * dt;

    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) camTarget += fwd * camSpeed;
    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) camTarget -= fwd * camSpeed;
    if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) camTarget += rgt * camSpeed;
    if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) camTarget -= rgt * camSpeed;
    if (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS) camTarget.y += camSpeed;
    if (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS) camTarget.y -= camSpeed;

    if (!paused) {
        float targetLateralSpeed = 0.0f;
        if (glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS)  targetLateralSpeed = -4.0f;
        if (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS) targetLateralSpeed = 4.0f;

        skierLateralSpeed = glm::mix(skierLateralSpeed, targetLateralSpeed, dt * 10.0f);
        skierLateral += skierLateralSpeed * dt;

        if (glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS) speedMul += 0.8f * dt;
        if (glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS) speedMul -= 0.8f * dt;

        skierLateral = glm::clamp(skierLateral, TRACK_MIN_X, TRACK_MAX_X);
        speedMul = glm::clamp(speedMul, -0.6f, 2.0f);
    }
}

void drawPiece(const Shader& sh, const Piece& p, const glm::mat4& model) {
    sh.setMat4("model", model);
    sh.setVec3("matAmbient", p.mAmb); sh.setVec3("matDiffuse", p.mDif); sh.setVec3("matSpecular", p.mSpec); sh.setFloat("matShininess", p.shin);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, p.tex);
    glBindVertexArray(p.mesh->VAO); glDrawElements(GL_TRIANGLES, p.mesh->indexCount, GL_UNSIGNED_INT, 0);
}
void drawPieceShadow(const Shader& sh, const Mesh& m, const glm::mat4& model) {
    sh.setMat4("model", model);
    glBindVertexArray(m.VAO); glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, 0);
}
void renderSolidObjects(const Shader& sh, bool shadowPass) {
    for (auto& p : staticPieces) { if (shadowPass) drawPieceShadow(sh, *p.mesh, p.local); else drawPiece(sh, p, p.local); }
    glm::mat4 skierRoot = getSkierTransform();
    for (auto& p : skierPieces) { glm::mat4 m = skierRoot * p.local; if (shadowPass) drawPieceShadow(sh, *p.mesh, m); else drawPiece(sh, p, m); }
    for (int i = 0; i < 2; i++) {
        glm::mat4 root = getCabinTransform(i);
        for (auto& p : cabinPieces) { glm::mat4 m = root * p.local; if (shadowPass) drawPieceShadow(sh, *p.mesh, m); else drawPiece(sh, p, m); }
    }
}

void setupScene(GLuint snowTex, GLuint woodTex, GLuint roofTex, GLuint rockTex, GLuint metalTex, GLuint redTex, GLuint blueTex) {
    staticPieces.push_back({ &gCube, snowTex, glm::translate(glm::mat4(1.0f), glm::vec3(0, -20.0f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(1000.0f, 2.0f, 1000.0f)), glm::vec3(0.6f,0.65f,0.7f), glm::vec3(1.0f), glm::vec3(0.3f), 16.0f });

    staticPieces.push_back({ &gTerrain, snowTex, glm::mat4(1.0f), glm::vec3(0.6f,0.65f,0.7f), glm::vec3(1.0f), glm::vec3(0.3f), 16.0f });

    staticPieces.push_back({ &gCone, snowTex, glm::translate(glm::mat4(1.0f), glm::vec3(-35, -8, -50)) * glm::scale(glm::mat4(1.0f), glm::vec3(30, 35, 30)), glm::vec3(0.5f,0.55f,0.6f), glm::vec3(0.9f,0.9f,0.95f), glm::vec3(0.2f), 8.0f });
    staticPieces.push_back({ &gCone, snowTex, glm::translate(glm::mat4(1.0f), glm::vec3(30, -8, -55)) * glm::scale(glm::mat4(1.0f), glm::vec3(25, 30, 25)), glm::vec3(0.5f,0.55f,0.6f), glm::vec3(0.9f,0.9f,0.95f), glm::vec3(0.2f), 8.0f });

    float chX = 6.0f, chZ = 6.0f, wallH = 2.5f;
    float chBase = heightAt(chX, chZ);

    float hCorner[4] = {
        heightAt(chX - 1.5f, chZ - 1.5f), heightAt(chX + 1.5f, chZ - 1.5f),
        heightAt(chX - 1.5f, chZ + 1.5f), heightAt(chX + 1.5f, chZ + 1.5f)
    };
    float chMinH = glm::min(glm::min(hCorner[0], hCorner[1]),
        glm::min(hCorner[2], hCorner[3]));
    float foundationDrop = (chBase - chMinH) + 0.15f;
    float wallTotalH = wallH + foundationDrop;

    staticPieces.push_back({ &gCube, woodTex,
        glm::translate(glm::mat4(1.0f),
            glm::vec3(chX, chBase + wallH * 0.5f - foundationDrop * 0.5f, chZ))
        * glm::scale(glm::mat4(1.0f), glm::vec3(3, wallTotalH, 3)),
        glm::vec3(0.4f,0.32f,0.25f), glm::vec3(1.0f), glm::vec3(0.2f), 8.0f });
    staticPieces.push_back({ &gPyramid, roofTex, glm::translate(glm::mat4(1.0f), glm::vec3(chX, chBase + wallH, chZ)) * glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0,1,0)) * glm::scale(glm::mat4(1.0f), glm::vec3(2.5f, 2.5f, 2.5f)), glm::vec3(0.3f,0.2f,0.2f), glm::vec3(1.0f), glm::vec3(0.4f), 32.0f });
    staticPieces.push_back({ &gCube, rockTex, glm::translate(glm::mat4(1.0f), glm::vec3(chX + 0.8f, chBase + wallH + 1.5f, chZ)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.4f, 1.6f, 0.4f)), glm::vec3(0.4f), glm::vec3(1.0f), glm::vec3(0.1f), 8.0f });
    staticPieces.push_back({ &gCube, woodTex, glm::translate(glm::mat4(1.0f), glm::vec3(chX + 1.5f, chBase + 0.6f, chZ)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.05f, 1.2f, 0.6f)), glm::vec3(0.3f,0.2f,0.15f), glm::vec3(1.0f), glm::vec3(0.1f), 8.0f });
    staticPieces.push_back({ &gCube, metalTex, glm::translate(glm::mat4(1.0f), glm::vec3(chX, chBase + 1.5f, chZ + 1.5f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.7f, 0.7f, 0.05f)), glm::vec3(0.4f,0.4f,0.5f), glm::vec3(1.0f), glm::vec3(0.9f), 64.0f });
    staticPieces.push_back({ &gCube, metalTex, glm::translate(glm::mat4(1.0f), glm::vec3(chX - 1.5f, chBase + 1.5f, chZ)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.05f, 0.7f, 0.7f)), glm::vec3(0.4f,0.4f,0.5f), glm::vec3(1.0f), glm::vec3(0.9f), 64.0f });
    staticPieces.push_back({ &gCube, metalTex, glm::translate(glm::mat4(1.0f), glm::vec3(chX, chBase + 1.5f, chZ - 1.5f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.7f, 0.7f, 0.05f)), glm::vec3(0.4f,0.4f,0.5f), glm::vec3(1.0f), glm::vec3(0.9f), 64.0f });

    std::vector<glm::vec2> trees = {
        {-15, 5}, {15, -5}, {8, 15}, {-8, 15}, {18, 0}, {12, 10}, {-12, -6}, {-18, -10}
    };
    for (auto& tp : trees) {
        float x = tp.x, z = tp.y, h = heightAt(x, z);
        staticPieces.push_back({ &gCylinder, woodTex, glm::translate(glm::mat4(1.0f), glm::vec3(x, h + 0.75f, z)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.15f, 1.5f, 0.15f)), glm::vec3(0.3f,0.2f,0.15f), glm::vec3(1.0f), glm::vec3(0.1f), 8.0f });
        staticPieces.push_back({ &gCone, rockTex, glm::translate(glm::mat4(1.0f), glm::vec3(x, h + 1.5f, z)) * glm::scale(glm::mat4(1.0f), glm::vec3(1.2f, 3.5f, 1.2f)), glm::vec3(0.2f,0.4f,0.2f), glm::vec3(0.5f,0.8f,0.5f), glm::vec3(0.1f), 8.0f });
    }

    float lx = -8.0f, lz1 = 12.0f, lz2 = -8.0f;
    float b1 = heightAt(lx, lz1), b2 = heightAt(lx, lz2), tH = 4.0f;

    staticPieces.push_back({ &gCylinder, metalTex, glm::translate(glm::mat4(1.0f), glm::vec3(lx, b1 + tH * 0.5f, lz1)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.2f, tH, 0.2f)), glm::vec3(0.5f), glm::vec3(1.0f), glm::vec3(0.6f), 32.0f });
    staticPieces.push_back({ &gCylinder, metalTex, glm::translate(glm::mat4(1.0f), glm::vec3(lx, b2 + tH * 0.5f, lz2)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.2f, tH, 0.2f)), glm::vec3(0.5f), glm::vec3(1.0f), glm::vec3(0.6f), 32.0f });

    staticPieces.push_back({ &gCylinder, metalTex, glm::translate(glm::mat4(1.0f), glm::vec3(lx, b1 + tH, lz1)) * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0,0,1)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.1f, 2.2f, 0.1f)), glm::vec3(0.5f), glm::vec3(1.0f), glm::vec3(0.6f), 32.0f });
    staticPieces.push_back({ &gCylinder, metalTex, glm::translate(glm::mat4(1.0f), glm::vec3(lx, b2 + tH, lz2)) * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0,0,1)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.1f, 2.2f, 0.1f)), glm::vec3(0.5f), glm::vec3(1.0f), glm::vec3(0.6f), 32.0f });

    cableFrom = glm::vec3(lx, b1 + tH, lz1);
    cableTo = glm::vec3(lx, b2 + tH, lz2);
    glm::vec3 cDir = cableTo - cableFrom; float cLen = glm::length(cDir); glm::vec3 cDn = cDir / cLen;
    glm::vec3 yA(0, 1, 0); glm::vec3 axis = glm::cross(yA, cDn);
    float angle = std::acos(glm::clamp(glm::dot(yA, cDn), -1.0f, 1.0f));
    glm::mat4 rot(1.0f); if (glm::length(axis) > 1e-4f) rot = glm::rotate(glm::mat4(1.0f), angle, glm::normalize(axis));

    staticPieces.push_back({ &gCylinder, metalTex, glm::translate(glm::mat4(1.0f), (cableFrom + cableTo) * 0.5f + glm::vec3(-1.0f, 0, 0)) * rot * glm::scale(glm::mat4(1.0f), glm::vec3(0.05f, cLen, 0.05f)), glm::vec3(0.4f), glm::vec3(1.0f), glm::vec3(0.3f), 16.0f });
    staticPieces.push_back({ &gCylinder, metalTex, glm::translate(glm::mat4(1.0f), (cableFrom + cableTo) * 0.5f + glm::vec3(1.0f, 0, 0)) * rot * glm::scale(glm::mat4(1.0f), glm::vec3(0.05f, cLen, 0.05f)), glm::vec3(0.4f), glm::vec3(1.0f), glm::vec3(0.3f), 16.0f });

    skierPieces.push_back({ &gCube, blueTex, glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.8f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.4f, 0.8f, 0.3f)), glm::vec3(0.2f,0.2f,0.3f), glm::vec3(1.0f), glm::vec3(0.3f), 16.0f });
    skierPieces.push_back({ &gCube, redTex, glm::translate(glm::mat4(1.0f), glm::vec3(0, 1.4f, -0.05f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.3f, 0.3f, 0.3f)), glm::vec3(0.4f,0.2f,0.2f), glm::vec3(1.0f), glm::vec3(0.3f), 16.0f });
    for (int s = -1; s <= 1; s += 2) {
        skierPieces.push_back({ &gCube, rockTex, glm::translate(glm::mat4(1.0f), glm::vec3(s * 0.15f, 0.25f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.1f, 0.4f, 0.15f)), glm::vec3(0.3f), glm::vec3(0.2f), glm::vec3(0.1f), 8.0f });
    }
    for (int s = -1; s <= 1; s += 2) {
        skierPieces.push_back({ &gCube, rockTex, glm::translate(glm::mat4(1.0f), glm::vec3(s * 0.15f, 0.05f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.1f, 0.05f, 1.6f)), glm::vec3(0.2f), glm::vec3(1.0f), glm::vec3(0.1f), 8.0f });
        skierPieces.push_back({ &gCylinder, metalTex, glm::translate(glm::mat4(1.0f), glm::vec3(s * 0.25f, 0.6f, 0.1f)) * glm::rotate(glm::mat4(1.0f), glm::radians(-15.0f), glm::vec3(1,0,0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.03f, 1.0f, 0.03f)), glm::vec3(0.3f), glm::vec3(1.0f), glm::vec3(0.5f), 32.0f });
    }

    cabinPieces.push_back({ &gCylinder, metalTex, glm::translate(glm::mat4(1.0f), glm::vec3(0, -0.2f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.04f, 0.4f, 0.04f)), glm::vec3(0.4f), glm::vec3(1.0f), glm::vec3(0.5f), 32.0f });
    cabinPieces.push_back({ &gCube, redTex, glm::translate(glm::mat4(1.0f), glm::vec3(0, -0.7f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.8f)), glm::vec3(0.4f,0.2f,0.2f), glm::vec3(1.0f), glm::vec3(0.5f), 32.0f });
    cabinPieces.push_back({ &gCone, roofTex, glm::translate(glm::mat4(1.0f), glm::vec3(0, -0.3f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.6f, 0.25f, 0.6f)), glm::vec3(0.3f,0.2f,0.2f), glm::vec3(1.0f), glm::vec3(0.4f), 32.0f });
}

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(SCR_W, SCR_H, "Ski Resort - KG&G", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window); glfwSwapInterval(1);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    glewExperimental = GL_TRUE; if (glewInit() != GLEW_OK) return -1;
    glEnable(GL_DEPTH_TEST); glEnable(GL_MULTISAMPLE); glEnable(GL_PROGRAM_POINT_SIZE);

    gCube = createCube(); gCylinder = createCylinder(24); gCone = createCone(24); gPyramid = createCone(4); gTerrain = createTerrain(200.0f, 100);

    GLuint snowTex = makeTexture(256, [](int x, int y) { float n = fbm(x * 0.08f, y * 0.08f, 42); float v = 0.92f + 0.08f * n; return glm::vec3(v * 0.95f, v * 0.97f, v); });
    GLuint woodTex = makeTexture(256, [](int x, int y) { float n = fbm(x * 0.2f, y * 0.06f, 7); float g = 0.5f + 0.5f * std::sin(y * 0.4f + n * 5.0f); return glm::vec3(0.32f + 0.14f * g + 0.08f * n, 0.18f + 0.08f * g + 0.04f * n, 0.08f + 0.03f * g); });
    GLuint roofTex = makeTexture(256, [](int x, int y) { float row = (y % 32) / 32.0f; float sh = 0.5f + 0.5f * row; float n = fbm(x * 0.1f, y * 0.1f, 13) * 0.2f; return glm::vec3(0.35f * sh + n, 0.08f * sh + n * 0.5f, 0.08f * sh + n * 0.5f); });
    GLuint rockTex = makeTexture(256, [](int x, int y) { float n = fbm(x * 0.15f, y * 0.15f, 99); float v = 0.25f + 0.5f * n; return glm::vec3(v * 0.9f, v * 0.85f, v * 0.8f); });
    GLuint metalTex = makeTexture(256, [](int x, int y) { float n = fbm(x * 0.05f, y * 0.05f, 3) * 0.15f; return glm::vec3(0.6f + n, 0.62f + n, 0.65f + n); });
    GLuint redTex = makeTexture(256, [](int x, int y) { float n = fbm(x * 0.08f, y * 0.08f, 5) * 0.15f; return glm::vec3(0.7f + n, 0.12f + n * 0.3f, 0.10f + n * 0.3f); });
    GLuint blueTex = makeTexture(256, [](int x, int y) { float n = fbm(x * 0.08f, y * 0.08f, 11) * 0.15f; return glm::vec3(0.15f + n, 0.3f + n, 0.7f + n); });

    Shader mainShader(MAIN_VS, MAIN_FS), shadowShader(SHADOW_VS, SHADOW_FS), snowShader(SNOW_VS, SNOW_FS);

    setupScene(snowTex, woodTex, roofTex, rockTex, metalTex, redTex, blueTex);

    GLuint depthMapFBO, depthMap;
    glGenFramebuffers(1, &depthMapFBO); glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_W, SHADOW_H, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[] = { 1,1,1,1 }; glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO); glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE); glBindFramebuffer(GL_FRAMEBUFFER, 0);

    srand(123);
    for (int i = 0; i < NUM_SNOW; i++) {
        SnowParticle p;
        p.pos = glm::vec3((rand() % 10000) / 100.0f - 50.0f, (rand() % 3000) / 100.0f, (rand() % 10000) / 100.0f - 50.0f);
        p.fallSpeed = 1.0f + (rand() % 100) / 100.0f; p.windX = 0.2f + (rand() % 100) / 500.0f; p.windZ = (rand() % 100) / 500.0f - 0.1f;
        snowParticles.push_back(p);
    }
    glGenVertexArrays(1, &snowVAO); glGenBuffers(1, &snowVBO);
    glBindVertexArray(snowVAO); glBindBuffer(GL_ARRAY_BUFFER, snowVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * NUM_SNOW, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0); glEnableVertexAttribArray(0);

    glm::mat4 lightProj = glm::ortho(-50.0f, 50.0f, -50.0f, 50.0f, 0.1f, 200.0f);
    glm::mat4 lightView = glm::lookAt(glm::vec3(-20.0f, 40.0f, 20.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightSpace = lightProj * lightView;

    mainShader.use();
    mainShader.setInt("diffuseTex", 0); mainShader.setInt("shadowMap", 1);
    mainShader.setVec3("dirLightDir", glm::vec3(0.38f, -0.79f, -0.48f));
    mainShader.setVec3("dirLightAmbient", glm::vec3(0.35f, 0.35f, 0.42f));
    mainShader.setVec3("dirLightDiffuse", glm::vec3(0.95f, 0.92f, 0.85f));
    mainShader.setVec3("dirLightSpecular", glm::vec3(1.0f));
    mainShader.setVec3("pointLightPos", glm::vec3(6.0f, heightAt(6, 6) + 1.5f, 8.0f));
    mainShader.setVec3("pointLightAmbient", glm::vec3(0.15f, 0.08f, 0.03f));
    mainShader.setVec3("pointLightDiffuse", glm::vec3(1.0f, 0.7f, 0.3f));
    mainShader.setVec3("pointLightSpecular", glm::vec3(1.0f, 0.7f, 0.3f));
    mainShader.setFloat("pointLightConst", 1.0f); mainShader.setFloat("pointLightLinear", 0.14f); mainShader.setFloat("pointLightQuad", 0.07f);
    mainShader.setVec3("fogColor", glm::vec3(0.55f, 0.75f, 0.95f));
    mainShader.setFloat("fogStart", 40.0f); mainShader.setFloat("fogEnd", 90.0f);


    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime(); float dt = (float)(now - lastTime); lastTime = now; if (dt > 0.1f) dt = 0.1f;

        processInput(window, dt);

        if (!paused) { animTime += dt; skierDistance += getSkierSpeed() * dt; updateSnow(dt); }

        shadowShader.use(); shadowShader.setMat4("lightSpaceMatrix", lightSpace);
        glViewport(0, 0, SHADOW_W, SHADOW_H); glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO); glClear(GL_DEPTH_BUFFER_BIT);
        renderSolidObjects(shadowShader, true); glBindFramebuffer(GL_FRAMEBUFFER, 0);

        int currentWidth, currentHeight;
        glfwGetFramebufferSize(window, &currentWidth, &currentHeight);
        
        glViewport(0, 0, currentWidth, currentHeight);
        glClearColor(0.55f, 0.75f, 0.95f, 1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::vec3 camPos = getCameraPos();
        glm::mat4 view = glm::lookAt(camPos, camTarget, glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)currentWidth / (float)currentHeight, 0.1f, 250.0f);

        mainShader.use(); mainShader.setMat4("view", view); mainShader.setMat4("projection", proj);
        mainShader.setMat4("lightSpaceMatrix", lightSpace); mainShader.setVec3("viewPos", camPos);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, depthMap);
        renderSolidObjects(mainShader, false);

        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
        snowShader.use(); snowShader.setMat4("view", view); snowShader.setMat4("projection", proj); snowShader.setFloat("pointSize", 8.0f);
        updateSnowVBO(); glBindVertexArray(snowVAO); glDrawArrays(GL_POINTS, 0, NUM_SNOW);
        glDepthMask(GL_TRUE); glDisable(GL_BLEND);

        glfwSwapBuffers(window); glfwPollEvents();
    }
    glfwTerminate(); return 0;
}
