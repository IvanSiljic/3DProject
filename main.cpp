#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <vector>
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "glm/glm.hpp"

unsigned int SCREEN_WIDTH = 1024;
unsigned int SCREEN_HEIGHT = 1024;
glm::vec3 c_lookFrom = glm::vec3(0,0,0);
glm::vec3 c_lookAt = glm::vec3(0, 0, -1);
glm::vec3 c_lookUp = glm::vec3(0, 1, 0);
float c_FOVDegrees = 60.f;
unsigned c_numBounces = 5;
float c_defocusAngle = 0;
float c_focusDist = 0.1;
bool c_sky = true;
GLuint frameCounter = 0;
GLuint c_samplesPerPixel = 1;
GLuint numOfSpheres = 1;
GLuint numOfQuads = 1;

const unsigned short OPENGL_MAJOR_VERSION = 4;
const unsigned short OPENGL_MINOR_VERSION = 3;
GLuint screenTex;
GLuint accumulationTex;
bool settingsChanged = false;

// Quad struct definition
struct Quad {
    glm::vec3 a;
    float reflectivity;
    glm::vec3 b;
    float fuzz;
    glm::vec3 c;
    float refractionIndex;
    glm::vec3 d;
    float padding;
    glm::vec3 normal;
    float padding1;
    glm::vec3 albedo;
    float padding2;
    glm::vec3 emission;
    float emissionStrength;
};

// Sphere struct definition
struct Sphere {
    glm::vec3 center;
    float radius;
    glm::vec3 albedo;
    float reflectivity;
    float fuzz;
    float refractionIndex;
    float padding[2];
    glm::vec3 emission;
    float emissionStrength;
};

GLfloat vertices[] =
        {
                -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
                1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
                1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };

GLuint indices[] = { 0, 2, 1, 0, 3, 2 };

// Function declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
std::string load_shader_code(const std::string& filepath);
GLuint compile_shader(const char* source, GLenum shaderType);
GLuint link_program(GLuint vertexShader, GLuint fragmentShader);
void setup_vertex_data(GLuint& VAO, GLuint& VBO, GLuint& EBO);
void setup_textures(GLuint& screenTex, GLuint& accumulationTex);
void setup_imgui(GLFWwindow* window);
void cleanup(GLFWwindow* window, GLuint VAO, GLuint VBO, GLuint EBO, GLuint screenTex, GLuint accumulationTex, GLuint quadProgram, GLuint computeProgram);

int main() {
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, OPENGL_MAJOR_VERSION);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, OPENGL_MINOR_VERSION);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "OpenGL RayTracer", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create the GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Set the framebuffer size callback
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize OpenGL context" << std::endl;
        return -1;
    }
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    setup_imgui(window);

    GLuint VAO, VBO, EBO;
    setup_vertex_data(VAO, VBO, EBO);
    setup_textures(screenTex, accumulationTex);

    // Create and populate spheres
    std::vector<Sphere> spheresData = {
            {glm::vec3(0.0,-0.499, -3), 0.5f, glm::vec3(1.0, 1.0, 1.0), 0.0f, 0.0f, 0, {0, 0}, glm::vec3(1,1,1), 0.0},
    };

    // Create and populate quads
    std::vector<Quad> quadsData = {
            // back side
            {
                glm::vec3(-1, -1, -4.0), 0,
                glm::vec3(-1,  1, -4.0), 0,
                glm::vec3( 1,  1, -4.0), 0,
                glm::vec3( 1, -1, -4.0), 0,
                glm::vec3(0.0,    0.0, 1.0), 0,
                glm::vec3(0.8, 0.8, 0.8), 0,
                glm::vec3(0.0, 0.0, 0.0), 0
            },
            // left side
            {
                glm::vec3(-1, -1, -4.0), 0,
                glm::vec3(-1,  1, -4.0), 0,
                glm::vec3(-1,  1, -2), 0,
                glm::vec3(-1, -1, -2), 0,
                glm::vec3(1.0,    0.0, 0.0), 0,
                glm::vec3(1.0, 0.0, 0.0), 0,
                glm::vec3(0.0, 0.0, 0.0), 0
            },
            // right side
            {
                glm::vec3(1, 1, -4.0), 0,
                glm::vec3(1, -1, -4.0), 0,
                glm::vec3(1, -1, -2), 0,
                glm::vec3(1, 1, -2), 0,
                glm::vec3(-1.0,    0.0, 0.0), 0,
                glm::vec3(0.0, 0.0, 1.0), 0,
                glm::vec3(0.0, 0.0, 0.0), 0
            },
            // bottom side
            {
                glm::vec3(-1, -1, -4.0), 0,
                glm::vec3(1, -1, -4.0), 0,
                glm::vec3(1, -1, -2), 0,
                glm::vec3(-1, -1, -2), 0,
                glm::vec3(0.0,    1.0, 0.0), 0,
                glm::vec3(0.0, 1.0, 0.0), 0,
                glm::vec3(0.0, 0.0, 0.0), 0
            },
            // top side
            {
                glm::vec3(-1, 1, -4.0), 0,
                glm::vec3(1, 1, -4.0), 0,
                glm::vec3(1, 1, -2), 0,
                glm::vec3(-1, 1, -2), 0,
                glm::vec3(0.0,    -1.0, 0.0), 0,
                glm::vec3(1.0, 1.0, 1.0), 0,
                glm::vec3(0.0, 0.0, 0.0), 0
            },
            // front side
            {
                glm::vec3(-1, -1, -2.0), 0,
                glm::vec3(-1,  1, -2.0), 0,
                glm::vec3( 1,  1, -2.0), 0,
                glm::vec3( 1, -1, -2.0), 0,
                glm::vec3(0.0,    0.0, -1.0), 0,
                glm::vec3(0.8, 0.8, 0.8), 0,
                glm::vec3(0.0, 0.0, 0.0), 0
            },
            // light
            {
                    glm::vec3(-0.5, 0.9, -4.0), 0,
                    glm::vec3(1,  0.9, -4.0), 0,
                    glm::vec3( 1,  0.9, -2.0), 0,
                    glm::vec3( -1, 0.9, -2.0), 0,
                    glm::vec3(0.0,    0.0, 1.0), 0,
                    glm::vec3(0.0, 0.0, 0.0), 0,
                    glm::vec3(1.0, 1.0, 1.0), 0.7
            }
    };

    numOfSpheres = spheresData.size();
    numOfQuads = quadsData.size();

    std::string vertexCode = load_shader_code("assets/shaders/vertex.glsl");
    std::string fragmentCode = load_shader_code("assets/shaders/fragment.glsl");
    std::string computeCode = load_shader_code("assets/shaders/compute.glsl");

    GLuint vertexShader = compile_shader(vertexCode.c_str(), GL_VERTEX_SHADER);
    GLuint fragmentShader = compile_shader(fragmentCode.c_str(), GL_FRAGMENT_SHADER);
    GLuint computeShader = compile_shader(computeCode.c_str(), GL_COMPUTE_SHADER);

    GLuint screenShaderProgram = link_program(vertexShader, fragmentShader);
    GLuint computeProgram = glCreateProgram();
    glAttachShader(computeProgram, computeShader);
    glLinkProgram(computeProgram);

    GLuint sphereBuffer;
    glGenBuffers(1, &sphereBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sphereBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, spheresData.size() * sizeof(Sphere), spheresData.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sphereBuffer); // Bind to binding point 0

    GLuint quadBuffer;
    glGenBuffers(1, &quadBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, quadBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, quadsData.size() * sizeof(Quad), quadsData.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, quadBuffer); // Bind to binding point 0

    glLinkProgram(computeProgram);

    // Set uniform variable locations
    GLint resolutionLocation = glGetUniformLocation(computeProgram, "iResolution");
    GLint lookFromLocation = glGetUniformLocation(computeProgram, "c_lookFrom");
    GLint lookAtLocation = glGetUniformLocation(computeProgram, "c_lookAt");
    GLint lookUpLocation = glGetUniformLocation(computeProgram, "c_lookUp");
    GLint FOVLocation = glGetUniformLocation(computeProgram, "c_FOVDegrees");
    GLint numBouncesLocation = glGetUniformLocation(computeProgram, "c_numBounces");
    GLint samplesPerPixelLocation = glGetUniformLocation(computeProgram, "c_samplesPerPixel");
    GLint defocusAngleLocation = glGetUniformLocation(computeProgram, "c_defocusAngle");
    GLint focalDistLocation = glGetUniformLocation(computeProgram, "c_focusDist");
    GLint skyLocation = glGetUniformLocation(computeProgram, "c_sky");
    GLint frameCounterLocation = glGetUniformLocation(computeProgram, "frameCounter");
    GLint numOfSpheresLocation = glGetUniformLocation(computeProgram, "numOfSpheres");
    GLint numOfQuadsLocation = glGetUniformLocation(computeProgram, "numOfQuads");

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Camera Settings Window
        ImGui::Begin("Camera Settings");

        static glm::vec3 prevLookFrom = c_lookFrom;
        static glm::vec3 prevLookAt = c_lookAt;
        static glm::vec3 prevLookUp = c_lookUp;
        static float prevFOV = c_FOVDegrees;
        static float prevDefocusAngle = c_defocusAngle;
        static float prevFocusDist = c_focusDist;
        static unsigned prevNumBounces = c_numBounces;
        static unsigned prevSamplesPerPixel = c_samplesPerPixel;
        static bool preSky = c_sky;

        ImGui::InputFloat3("Position", reinterpret_cast<float *>(&c_lookFrom));
        ImGui::InputFloat3("Orientation", reinterpret_cast<float *>(&c_lookAt));
        ImGui::InputFloat3("Rotation", reinterpret_cast<float *>(&c_lookUp));
        ImGui::SliderFloat("Field of View", &c_FOVDegrees, 1.0f, 180.0f);
        ImGui::SliderFloat("Defocus Angle", &c_defocusAngle, 0.0f, 10.0f);
        ImGui::SliderFloat("Focus Distance", &c_focusDist, 0.1f, 50.0f);
        ImGui::SliderInt("Number of Bounces", (int*)&c_numBounces, 1, 30);
        ImGui::SliderInt("Samples per Pixel", (int*)&c_samplesPerPixel, 1, 20);
        ImGui::Checkbox("Sky", &c_sky);

        if (prevLookFrom != c_lookFrom || prevLookAt != c_lookAt || prevLookUp != c_lookUp || c_FOVDegrees != prevFOV || c_numBounces != prevNumBounces || prevDefocusAngle != c_defocusAngle || prevFocusDist != c_focusDist || prevSamplesPerPixel != c_samplesPerPixel || preSky != c_sky) {
            settingsChanged = true;
            prevLookFrom = c_lookFrom;
            prevLookAt = c_lookAt;
            prevLookUp = c_lookUp;
            prevFOV = c_FOVDegrees;
            prevDefocusAngle = c_defocusAngle;
            prevFocusDist = c_focusDist;
            prevNumBounces = c_numBounces;
            prevSamplesPerPixel = c_samplesPerPixel;
            preSky = c_sky;
        }

        ImGui::End();

        // Scene Settings Window
        ImGui::Begin("Scene Settings");

        if (ImGui::Button("Add Sphere")) {
            Sphere s;
            s.center = glm::vec3(0.0f);
            s.radius = 1.0f; // radius
            s.albedo = glm::vec3(1.0f);
            s.reflectivity = 0;
            s.fuzz = 0;
            s.refractionIndex = 0;
            s.emission = glm::vec3(0.0f);

            spheresData.push_back(s);

            numOfSpheres++;
            settingsChanged = true;
        }

        if (ImGui::Button("Add Quad")) {
            Quad q;
            q.a = glm::vec3(0.0f);
            q.b = glm::vec3(0.0f);
            q.c = glm::vec3(0.0f);
            q.d = glm::vec3(0.0f);
            q.normal = glm::vec3(0.0f);
            q.albedo = glm::vec3(1.0f);
            q.reflectivity = 0;
            q.fuzz = 0;
            q.refractionIndex = 0;
            q.emission = glm::vec3(0.0f);

            quadsData.push_back(q);

            numOfQuads++;
            settingsChanged = true;
        }

        for (unsigned int i = 0; i < numOfSpheres; ++i) {
            std::string sphereLabel = "Sphere " + std::to_string(i);
            if (ImGui::TreeNode(sphereLabel.c_str())) {
                ImGui::InputFloat3(("Position##" + std::to_string(i)).c_str(), reinterpret_cast<float *>(&spheresData[i].center));
                ImGui::InputFloat(("Radius##" + std::to_string(i)).c_str(), &spheresData[i].radius);
                ImGui::ColorEdit3(("Albedo##" + std::to_string(i)).c_str(), reinterpret_cast<float *>(&spheresData[i].albedo));
                ImGui::SliderFloat(("Reflectivity##" + std::to_string(i)).c_str(), &spheresData[i].reflectivity, 0.0f, 1.0f);
                ImGui::SliderFloat(("Fuzz##" + std::to_string(i)).c_str(), &spheresData[i].fuzz, 0.0f, 1.0f);
                ImGui::InputFloat(("Refraction Index##" + std::to_string(i)).c_str(), &spheresData[i].refractionIndex);
                ImGui::ColorEdit3(("Emission##" + std::to_string(i)).c_str(), reinterpret_cast<float *>(&spheresData[i].emission)); // New emission input
                ImGui::InputFloat(("Emission Strength##" + std::to_string(i)).c_str(), &spheresData[i].emissionStrength);

                if (ImGui::Button(("Remove##" + std::to_string(i)).c_str())) {
                    for (unsigned int j = i; j < numOfSpheres - 1; ++j) {
                        spheresData[j] = spheresData[j + 1];
                    }
                    spheresData.pop_back();
                    numOfSpheres--;
                    settingsChanged = true;
                }
                if (ImGui::Button(("Apply"))) {
                    settingsChanged = true;
                }
                ImGui::TreePop();
            }
        }

        for (unsigned int i = 0; i < numOfQuads; ++i) {
            std::string quadLabel = "Quad " + std::to_string(i);
            if (ImGui::TreeNode(quadLabel.c_str())) {
                ImGui::InputFloat3(("A##" + std::to_string(i)).c_str(), reinterpret_cast<float *>(&quadsData[i].a));
                ImGui::InputFloat3(("B##" + std::to_string(i)).c_str(), reinterpret_cast<float *>(&quadsData[i].b));
                ImGui::InputFloat3(("C##" + std::to_string(i)).c_str(), reinterpret_cast<float *>(&quadsData[i].c));
                ImGui::InputFloat3(("D##" + std::to_string(i)).c_str(), reinterpret_cast<float *>(&quadsData[i].d));
                ImGui::InputFloat3(("Normal##" + std::to_string(i)).c_str(), reinterpret_cast<float *>(&quadsData[i].normal));
                ImGui::ColorEdit3(("Albedo##" + std::to_string(i)).c_str(), reinterpret_cast<float *>(&quadsData[i].albedo));
                ImGui::SliderFloat(("Reflectivity##" + std::to_string(i)).c_str(), &quadsData[i].reflectivity, 0.0f, 1.0f);
                ImGui::SliderFloat(("Fuzz##" + std::to_string(i)).c_str(), &quadsData[i].fuzz, 0.0f, 1.0f);
                ImGui::InputFloat(("Refraction Index##" + std::to_string(i)).c_str(), &quadsData[i].refractionIndex);
                ImGui::ColorEdit3(("Emission Color##" + std::to_string(i)).c_str(), reinterpret_cast<float *>(&quadsData[i].emission)); // New emission input
                ImGui::InputFloat(("Emission Strength##" + std::to_string(i)).c_str(), &quadsData[i].emissionStrength);

                if (ImGui::Button(("Remove##" + std::to_string(i)).c_str())) {
                    for (unsigned int j = i; j < numOfQuads - 1; ++j) {
                        quadsData[j] = quadsData[j + 1];
                    }
                    quadsData.pop_back();
                    numOfQuads--;
                    settingsChanged = true;
                }
                if (ImGui::Button(("Apply"))) {
                    settingsChanged = true;
                }
                ImGui::TreePop();
            }
        }
        ImGui::End();

        ImGui::Render();

        if (settingsChanged) {
            glUseProgram(computeProgram);
            // Update sphere buffer
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, sphereBuffer);
            glBufferData(GL_SHADER_STORAGE_BUFFER, spheresData.size() * sizeof(Sphere), spheresData.data(), GL_STATIC_DRAW);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sphereBuffer); // Bind to binding point 0
            // Update quad buffer
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, quadBuffer);
            glBufferData(GL_SHADER_STORAGE_BUFFER, quadsData.size() * sizeof(Quad), quadsData.data(), GL_STATIC_DRAW);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, quadBuffer); // Bind to binding point 0

            frameCounter = 0;
            settingsChanged = false;
            // Clear the accumulation buffer
            float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            glClearTexImage(accumulationTex, 0, GL_RGBA, GL_FLOAT, clearColor);
            // Rebind accumulation texture
            glBindImageTexture(1, accumulationTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
        }
        glUseProgram(computeProgram);
        glUniform2i(resolutionLocation, SCREEN_WIDTH, SCREEN_HEIGHT);
        glUniform3f(lookFromLocation, c_lookFrom.x, c_lookFrom.y, c_lookFrom.z);
        glUniform3f(lookAtLocation, c_lookAt.x, c_lookAt.y, c_lookAt.z);
        glUniform3f(lookUpLocation, c_lookUp.x, c_lookUp.y, c_lookUp.z);
        glUniform1f(FOVLocation, static_cast<GLfloat>(c_FOVDegrees));
        glUniform1f(defocusAngleLocation, static_cast<GLfloat>(c_defocusAngle));
        glUniform1f(focalDistLocation, static_cast<GLfloat>(c_focusDist));
        glUniform1ui(samplesPerPixelLocation, c_samplesPerPixel);
        glUniform1ui(numBouncesLocation, c_numBounces);
        glUniform1ui(frameCounterLocation, frameCounter);
        glUniform1ui(numOfSpheresLocation, numOfSpheres);
        glUniform1ui(numOfQuadsLocation, numOfQuads);
        glUniform1i(skyLocation, static_cast<int>(c_sky));

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sphereBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, quadBuffer);
        glDispatchCompute((int)(SCREEN_WIDTH / 8), (int)(SCREEN_HEIGHT / 4), 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);

        frameCounter++;

        glUseProgram(screenShaderProgram);
        glBindTextureUnit(0, screenTex);
        glUniform1i(glGetUniformLocation(screenShaderProgram, "screen"), 0);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, sizeof(indices) / sizeof(indices[0]), GL_UNSIGNED_INT, 0);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    cleanup(window, VAO, VBO, EBO, screenTex, accumulationTex, screenShaderProgram, computeProgram);
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    // Update viewport when window is resized
    glViewport(0, 0, width, height);
    SCREEN_WIDTH = width;
    SCREEN_HEIGHT = height;
    frameCounter = 0;

    // Recreate screen and accumulation textures with new dimensions
    glDeleteTextures(1, &screenTex);
    glCreateTextures(GL_TEXTURE_2D, 1, &screenTex);
    glTextureParameteri(screenTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(screenTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(screenTex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(screenTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureStorage2D(screenTex, 1, GL_RGBA32F, width, height);
    glBindImageTexture(0, screenTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    glDeleteTextures(1, &accumulationTex);
    glCreateTextures(GL_TEXTURE_2D, 1, &accumulationTex);
    glTextureParameteri(accumulationTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(accumulationTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(accumulationTex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(accumulationTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureStorage2D(accumulationTex, 1, GL_RGBA32F, width, height);
    glBindImageTexture(1, accumulationTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
}

void setup_imgui(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430");
}

void setup_vertex_data(GLuint& VAO, GLuint& VBO, GLuint& EBO) {
    glCreateVertexArrays(1, &VAO);
    glCreateBuffers(1, &VBO);
    glCreateBuffers(1, &EBO);

    glNamedBufferData(VBO, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glNamedBufferData(EBO, sizeof(indices), indices, GL_STATIC_DRAW);

    glEnableVertexArrayAttrib(VAO, 0);
    glVertexArrayAttribBinding(VAO, 0, 0);
    glVertexArrayAttribFormat(VAO, 0, 3, GL_FLOAT, GL_FALSE, 0);

    glEnableVertexArrayAttrib(VAO, 1);
    glVertexArrayAttribBinding(VAO, 1, 0);
    glVertexArrayAttribFormat(VAO, 1, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat));

    glVertexArrayVertexBuffer(VAO, 0, VBO, 0, 5 * sizeof(GLfloat));
    glVertexArrayElementBuffer(VAO, EBO);
}

void setup_textures(GLuint& screenTex, GLuint& accumulationTex) {
    glCreateTextures(GL_TEXTURE_2D, 1, &screenTex);
    glTextureParameteri(screenTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(screenTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(screenTex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(screenTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureStorage2D(screenTex, 1, GL_RGBA32F, SCREEN_WIDTH, SCREEN_HEIGHT);
    glBindImageTexture(0, screenTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    glCreateTextures(GL_TEXTURE_2D, 1, &accumulationTex);
    glTextureParameteri(accumulationTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(accumulationTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(accumulationTex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(accumulationTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureStorage2D(accumulationTex, 1, GL_RGBA32F, SCREEN_WIDTH, SCREEN_HEIGHT);
    glBindImageTexture(1, accumulationTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

}

std::string load_shader_code(const std::string& filepath) {
    std::ifstream shaderFile(filepath);
    std::stringstream shaderStream;
    shaderStream << shaderFile.rdbuf();
    shaderFile.close();
    return shaderStream.str();
}

GLuint compile_shader(const char* source, GLenum shaderType) {
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(logLength);
        glGetShaderInfoLog(shader, logLength, &logLength, log.data());
        std::cerr << "Error compiling shader: " << log.data() << std::endl;
    }

    return shader;
}

GLuint link_program(GLuint vertexShader, GLuint fragmentShader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(logLength);
        glGetProgramInfoLog(program, logLength, &logLength, log.data());
        std::cerr << "Error linking program: " << log.data() << std::endl;
    }
    return program;
}

void cleanup(GLFWwindow* window, GLuint VAO, GLuint VBO, GLuint EBO, GLuint screenTex, GLuint accumulationTex, GLuint quadProgram, GLuint computeProgram) {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteTextures(1, &screenTex);
    glDeleteTextures(1, &accumulationTex);
    glDeleteProgram(quadProgram);
    glDeleteProgram(computeProgram);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}
