#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "glad/glad.h"
#include <GLFW/glfw3.h>
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

const unsigned short OPENGL_MAJOR_VERSION = 4;
const unsigned short OPENGL_MINOR_VERSION = 3;
GLuint screenTex;
GLuint accumulationTex;

bool settingsChanged = false;

#define MAX_SPHERES 2 // Make sure to change in assets/shaders/compute.glsl
#define SPHERE_DATA_POINTS 7

typedef float SphereData[SPHERE_DATA_POINTS];

// Generate sphere data
SphereData sphereData[MAX_SPHERES] = {
        {2, 0, -10, 1, 0, 0, 1},
        {0, 0, -10, 1, 1, 0, 0},
        /*
        {0, 0.5, -10, 1, 0, 1, 0},
        {2, 2, -10, 1, 1, 0, 1},
        {{-2, 0, -10}, 1, {0, 1, 1}},
        {{0, -2, -10}, 1, {1, 1, 0}},
        {{-2, -2, -10}, 1, {1, 1, 1}},
        {{-2, 2, -10}, 1, {0.5, 0.5, 0.5}},
        {{2, -2, -10}, 1, {1, 0.2, 0.5}},
        {{2, -2, -10}, 1, {0.8, 0.9, 0.9}}
         */
};

GLfloat vertices[] =
        {
                -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
                1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
                1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };

GLuint indices[] =
        {
                0, 2, 1,
                0, 3, 2
        };

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

int main()
{
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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    GLuint VAO, VBO, EBO, SSBO;
    glCreateVertexArrays(1, &VAO);
    glCreateBuffers(1, &VBO);
    glCreateBuffers(1, &EBO);
    glGenBuffers(1, &SSBO);

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

    // Binding spheres
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, SSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_SPHERES * sizeof(SphereData), sphereData, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, SSBO);

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

    std::string vertexCode;
    std::string fragmentCode;
    std::string computeCode;
    std::ifstream vShaderFile;
    std::ifstream fShaderFile;
    std::ifstream cShaderFile;
    // ensure ifstream objects can throw exceptions:
    vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try
    {
        // open files
        vShaderFile.open("assets/shaders/vertex.glsl");
        fShaderFile.open("assets/shaders/fragment.glsl");
        cShaderFile.open("assets/shaders/compute.glsl");
        std::stringstream vShaderStream, fShaderStream, cShaderStream;
        // read file's buffer contents into streams
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();
        cShaderStream << cShaderFile.rdbuf();
        // close file handlers
        vShaderFile.close();
        fShaderFile.close();
        cShaderFile.close();
        // convert stream into string
        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
        computeCode = cShaderStream.str();
    }
    catch (std::ifstream::failure &e)
    {
        std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
    }
    const char *vShaderCode = vertexCode.c_str();
    const char *fShaderCode = fragmentCode.c_str();
    const char *cShaderCode = computeCode.c_str();

    GLuint screenVertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(screenVertexShader, 1, &vShaderCode, NULL);
    glCompileShader(screenVertexShader);
    GLuint screenFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(screenFragmentShader, 1, &fShaderCode, NULL);
    glCompileShader(screenFragmentShader);

    GLuint screenShaderProgram = glCreateProgram();
    glAttachShader(screenShaderProgram, screenVertexShader);
    glAttachShader(screenShaderProgram, screenFragmentShader);
    glLinkProgram(screenShaderProgram);

    glDeleteShader(screenVertexShader);
    glDeleteShader(screenFragmentShader);

    GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(computeShader, 1, &cShaderCode, NULL);
    glCompileShader(computeShader);

    // Check for compilation errors
    GLint compileStatus;
    glGetShaderiv(computeShader, GL_COMPILE_STATUS, &compileStatus);
    if (compileStatus != GL_TRUE) {
        GLint infoLogLength;
        glGetShaderiv(computeShader, GL_INFO_LOG_LENGTH, &infoLogLength);
        if (infoLogLength > 0) {
            GLchar infoLog[infoLogLength + 1];
            glGetShaderInfoLog(computeShader, infoLogLength, NULL, infoLog);
            std::cerr << "Error compiling compute shader: \n" << infoLog << std::endl;
        }
    }

    GLuint computeProgram = glCreateProgram();
    glAttachShader(computeProgram, computeShader);
    glLinkProgram(computeProgram);

    // Set uniform variable locations
    GLint resolutionLocation = glGetUniformLocation(computeProgram, "iResolution");
    GLint FOVLocation = glGetUniformLocation(computeProgram, "c_FOVDegrees");
    GLint focalLengthLocation = glGetUniformLocation(computeProgram, "c_focalLength");
    GLint numBouncesLocation = glGetUniformLocation(computeProgram, "c_numBounces");
    GLint samplesPerPixelLocation = glGetUniformLocation(computeProgram, "samplesPerPixel");
    GLint frameCounterLocation = glGetUniformLocation(computeProgram, "frameCounter");

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

        ImGui::SliderFloat("Field of View", &c_FOVDegrees, 1.0f, 180.0f);
        ImGui::SliderFloat("Focal Length", &c_focalLength, 0.1f, 10.0f);
        ImGui::SliderInt("Number of Bounces", (int*)&c_numBounces, 1, 10);
        ImGui::SliderInt("Samples per Pixel", (int*)&samplesPerPixel, 1, 20);

        if (c_FOVDegrees != prevFOV || c_numBounces != prevNumBounces || prevFocalLength != c_focalLength || prevSamplesPerPixel != samplesPerPixel) {
            settingsChanged = true;
            prevFOV = c_FOVDegrees;
            prevNumBounces = c_numBounces;
        }

        ImGui::End();

        ImGui::Render();

        glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(computeProgram);
        glUniform2i(resolutionLocation, SCREEN_WIDTH, SCREEN_HEIGHT);
        glUniform1f(FOVLocation, static_cast<GLfloat>(c_FOVDegrees));
        glUniform1f(focalLengthLocation, static_cast<GLfloat>(c_focalLength));
        glUniform1ui(samplesPerPixelLocation, samplesPerPixel);
        glUniform1ui(numBouncesLocation, c_numBounces);

        if (settingsChanged) {
            frameCounter = 0;
            settingsChanged = false;
            // Clear the accumulation buffer
            glClearTexImage(accumulationTex, 0, GL_RGBA, GL_FLOAT, nullptr);
        }

        glUniform1ui(frameCounterLocation, frameCounter);

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

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}
