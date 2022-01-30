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
#include <memory>
#include <filesystem>
#include "AnimatedModel.h"
#include "Camera.h"
#include "ClipPickScene.h"
#include "Input.h"
#include "Light.h"
#include "PoseEditScene.h"  
#include "Scene.h"
#include "Shader.h"


// TODO: remove these globals
int windowWidth = 1200;
int windowHeight = 800;
 
static void GLFWErrorCallback(int error, const char* description)
{
    std::cerr << "Glfw Error " << error << ": " << description << '\n';
}

void FramebufferSizeCallback(GLFWwindow*, int width, int height)
{
    glViewport(0, 0, width, height);
    windowWidth = width;
    windowHeight = height;
}

void ProcessInput(GLFWwindow* window, Input& out_input, const ImGuiIO& io)
{
    static bool first_poll = true;

    auto prev_mouse_x = out_input.mouse_x;
    auto prev_mouse_y = out_input.mouse_y;
    double current_mouse_x, current_mouse_y;
    glfwGetCursorPos(window, &current_mouse_x, &current_mouse_y);
    out_input.mouse_x = (float)current_mouse_x;
    out_input.mouse_y = (float)current_mouse_y;

    glfwGetWindowSize(window, &out_input.window_width, &out_input.window_height);

    if (first_poll)
    {
        out_input.mouse_delta_x = 0.0;
        out_input.mouse_delta_y = 0.0;
        first_poll = false;
    }
    else
    {
        if (!io.WantCaptureMouse)
        {
            out_input.mouse_delta_x = out_input.mouse_x - prev_mouse_x;
            out_input.mouse_delta_y = prev_mouse_y - out_input.mouse_y;
        }
        else
        {
            out_input.mouse_delta_x = 0.0;
            out_input.mouse_delta_y = 0.0;
        }
    }

    if (!io.WantCaptureMouse)
    {
        out_input.left_mouse_pressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    }
    else
    {
        out_input.left_mouse_pressed = false;
    }
    
    if (!io.WantCaptureKeyboard)
    {
        out_input.w_pressed = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
        out_input.s_pressed = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        out_input.a_pressed = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
        out_input.d_pressed = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
    }
    else
    {
        out_input.w_pressed = false;
        out_input.s_pressed = false;
        out_input.a_pressed = false;
        out_input.d_pressed = false;
    }
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
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    std::vector<AnimatedModel> models;

    namespace fs = std::filesystem;

    const auto models_directory = fs::path("Models");
    assert(fs::is_directory(models_directory));
    fs::path model_file_path, skeleton_file_path;
    std::vector<fs::path> animation_file_paths;
    for (const auto& dir_entry : fs::directory_iterator(models_directory))
    {
        models.emplace_back(dir_entry.path().string());
    }

    float deltaTime = 0.0f;
    float lastFrameTime = 0.0f;

    Input input{};

    Shader model_shader{ "Shaders/anim.vert", "Shaders/anim.frag", nullptr, 
        {
            {
                .uniform_block_name = "Matrices",
                .uniform_block_binding = 0
            },
            {
                .uniform_block_name = "Lights",
                .uniform_block_binding = 1
            }
        }
    };

    std::unique_ptr<Scene> scene = std::make_unique<PoseEditScene>(models, projViewUBO, lightsUBO, model_shader);

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        float currentTime = (float)glfwGetTime();
        deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        ProcessInput(window, input, io);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Rendering
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        scene->UpdateAndRender(input, deltaTime);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
