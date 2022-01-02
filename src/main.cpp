// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include <glad/glad.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <iostream>
#include "AnimatedModel.h"
#include "Camera.h"
#include "Light.h"
#include "Shader.h"

int windowWidth = 1200;
int windowHeight = 800;

static void GLFWErrorCallback(int error, const char* description)
{
    std::cerr << "Glfw Error " << error << ": " << description << '\n';
}

void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    windowWidth = width;
    windowHeight = height;
}

void ProcessInput(GLFWwindow* window)
{
}

int main(int, char**)
{
    // Setup window
    glfwSetErrorCallback(GLFWErrorCallback);
    if (!glfwInit())
        return 1;

    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "anim_view", NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        return -1;
    }   

    glViewport(0, 0, windowWidth, windowHeight);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    //glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    GLuint projViewUBO, lightsUBO;
    constexpr int maxPointLights = 25;
    constexpr int maxSpotLights = 25;

    glGenBuffers(1, &projViewUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, projViewUBO);
    glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, projViewUBO);

    glGenBuffers(1, &lightsUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, lightsUBO);
    glBufferData(GL_UNIFORM_BUFFER, maxPointLights * sizeof(PointLight) + maxSpotLights * sizeof(SpotLight) + sizeof(DirectionalLight) + 2 * sizeof(int), NULL, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, lightsUBO);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    AnimatedModel warrok("Models/Warrok2/Warrok.model");
    Camera camera({ 0.0f, 1.5f, 3.0f });

    Shader shader("Shaders/anim.vert", "Shaders/anim.frag");
    shader.use();
    GLuint shaderBlockIndex = glGetUniformBlockIndex(shader.id, "Matrices");
    glUniformBlockBinding(shader.id, shaderBlockIndex, 0);
    GLuint shaderLightsBlockIndex = glGetUniformBlockIndex(shader.id, "Lights");
    glUniformBlockBinding(shader.id, shaderLightsBlockIndex, 1);

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        static int pose = 0;
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Animation Select");
            
            ImGui::InputInt("Pose (0 to 250)", &pose);
            if (pose < 0) pose = 250;
            if (pose > 250) pose = 0;

            ImGui::End();
        }
        
        auto view = camera.GetViewMatrix();
        auto aspect = (float)windowWidth / (float)windowHeight;
        auto proj = camera.GetProjectionMatrix(aspect);

        glBindBuffer(GL_UNIFORM_BUFFER, projViewUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4),
            glm::value_ptr(proj));
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4),
            glm::value_ptr(view));

        glm::mat4 model = glm::identity<glm::mat4>();
        model = glm::translate(model, glm::vec3(0, 1.0, 0));
        model = glm::scale(model, glm::vec3(0.01f, 0.01f, 0.01f));
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(view * model));
        shader.SetMat4("model", glm::value_ptr(model));
        glUniformMatrix3fv(glGetUniformLocation(shader.id, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));

        DirectionalLight dirLightViewSpace = DirectionalLight(glm::vec3(0.2f, 0.2f, 0.2f), glm::vec3(0.8, 0.8f, 0.8f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f));
        int numLights[2] = { 0, 0 };
        glBindBuffer(GL_UNIFORM_BUFFER, lightsUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, maxPointLights * sizeof(PointLight) + maxSpotLights * sizeof(SpotLight) + sizeof(DirectionalLight), sizeof(numLights), numLights);
        glBufferSubData(GL_UNIFORM_BUFFER, maxPointLights * sizeof(PointLight) + maxSpotLights * sizeof(SpotLight), sizeof(DirectionalLight), &dirLightViewSpace);

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        warrok.Draw(shader, pose);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
