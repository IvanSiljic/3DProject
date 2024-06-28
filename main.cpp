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
float c_FOVDegrees = 90.f;
unsigned c_numBounces = 3;
float c_focalLength = 1.f;
GLuint frameCounter = 0;
GLuint samplesPerPixel = 5;
GLuint numOfSpheres = 5;

const unsigned short OPENGL_MAJOR_VERSION = 4;
const unsigned short OPENGL_MINOR_VERSION = 3;
GLuint screenTex;
GLuint accumulationTex;
bool settingsChanged = false;

// Sphere struct definition
struct Sphere {
    glm::vec3 center;
    float radius;
    glm::vec3 albedo;
    float reflectivity;
    float fuzz;
    float refractionIndex;
    float padding[2]; // Padding to ensure alignment
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
            {glm::vec3(0.0, -100.5, -1.0), 100.0f, glm::vec3(0.8, 0.8, 0.0), 0.0f, 0.0f, 0.0},
            {glm::vec3(0.0,    0.0, -1.2), 0.5f, glm::vec3(0.1, 0.2, 0.5), 0.0f, 0.0f, 1.5},
            {glm::vec3(0.0,    0.0, -1.2), 0.4f, glm::vec3(0.1, 0.2, 0.5), 0.0f, 0.0f, 1.00 / 1.50},
            {glm::vec3(-1.0,    0.0, -1.0), 0.5f, glm::vec3(0.8, 0.8, 0.8), 1.0f, 0.3f, 0.0},
            {glm::vec3(1.0,    0.0, -1.0), 0.5f, glm::vec3(0.8, 0.6, 0.2), 1.0f, 1.0f, 0.0}
    };

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

    glLinkProgram(computeProgram);

    // Set uniform variable locations
    GLint resolutionLocation = glGetUniformLocation(computeProgram, "iResolution");
    GLint FOVLocation = glGetUniformLocation(computeProgram, "c_FOVDegrees");
    GLint numBouncesLocation = glGetUniformLocation(computeProgram, "c_numBounces");
    GLint focalLengthLocation = glGetUniformLocation(computeProgram, "c_focalLength");
    GLint frameCounterLocation = glGetUniformLocation(computeProgram, "frameCounter");
    GLint samplesPerPixelLocation = glGetUniformLocation(computeProgram, "samplesPerPixel");
    GLint numOfSpheresLocation = glGetUniformLocation(computeProgram, "numOfSpheres");

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Create a window using ImGui
        ImGui::Begin("Settings");

        // Add ImGui controls here
        static float prevFOV = c_FOVDegrees;
        static float prevFocalLength = c_focalLength;
        static unsigned prevNumBounces = c_numBounces;
        static unsigned prevSamplesPerPixel = samplesPerPixel;
        static unsigned prevNumOfSpheres = numOfSpheres;

        ImGui::SliderFloat("Field of View", &c_FOVDegrees, 1.0f, 180.0f);
        ImGui::SliderFloat("Focal Length", &c_focalLength, 0.1f, 10.0f);
        ImGui::SliderInt("Number of Bounces", (int*)&c_numBounces, 1, 30);
        ImGui::SliderInt("Samples per Pixel", (int*)&samplesPerPixel, 1, 20);

        if (c_FOVDegrees != prevFOV || c_numBounces != prevNumBounces || prevFocalLength != c_focalLength || prevSamplesPerPixel != samplesPerPixel) {
            settingsChanged = true;
            prevFOV = c_FOVDegrees;
            prevFocalLength = c_focalLength;
            prevNumBounces = c_numBounces;
            prevSamplesPerPixel = samplesPerPixel;
            prevNumOfSpheres = numOfSpheres;
        }

        if (ImGui::Button("Add Sphere")) {
            Sphere s;
            s.center = glm::vec3(0.0f);
            s.radius = 1.0f; // radius
            s.albedo = glm::vec3(1.0f);
            s.reflectivity = 0;
            s.fuzz = 0;
            s.refractionIndex = 0;

            spheresData.push_back(s);

            numOfSpheres++;
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

        ImGui::End();

        ImGui::Render();

        if (settingsChanged) {
            glUseProgram(computeProgram);
            // Update sphere buffer
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, sphereBuffer);
            glBufferData(GL_SHADER_STORAGE_BUFFER, spheresData.size() * sizeof(Sphere), spheresData.data(), GL_STATIC_DRAW);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sphereBuffer); // Bind to binding point 0

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
        glUniform1f(FOVLocation, static_cast<GLfloat>(c_FOVDegrees));
        glUniform1f(focalLengthLocation, static_cast<GLfloat>(c_focalLength));
        glUniform1ui(samplesPerPixelLocation, samplesPerPixel);
        glUniform1ui(numBouncesLocation, c_numBounces);
        glUniform1ui(frameCounterLocation, frameCounter);
        glUniform1ui(numOfSpheresLocation, numOfSpheres);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sphereBuffer);
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
