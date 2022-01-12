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
#include "Light.h"
#include "Shader.h"

int windowWidth = 1200;
int windowHeight = 800;

struct Input
{
    float mouse_x;
    float mouse_y;
    float mouse_delta_x;
    float mouse_delta_y;

    bool w_pressed, a_pressed, s_pressed, d_pressed;
    bool left_mouse_pressed;
};
 
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

void ProcessInput(GLFWwindow* window, Input& out_input, const ImGuiIO& io)
{
    static bool first_poll = true;
    auto prev_mouse_x = out_input.mouse_x;
    auto prev_mouse_y = out_input.mouse_y;
    double current_mouse_x, current_mouse_y;
    glfwGetCursorPos(window, &current_mouse_x, &current_mouse_y);
    out_input.mouse_x = (float)current_mouse_x;
    out_input.mouse_y = (float)current_mouse_y;

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

    Camera camera({ 0.0f, 1.5f, 5.0f });

    Shader shader("Shaders/anim.vert", "Shaders/anim.frag");
    shader.use();
    GLuint shaderBlockIndex = glGetUniformBlockIndex(shader.id, "Matrices");
    glUniformBlockBinding(shader.id, shaderBlockIndex, 0);
    GLuint shaderLightsBlockIndex = glGetUniformBlockIndex(shader.id, "Lights");
    glUniformBlockBinding(shader.id, shaderLightsBlockIndex, 1);

    float deltaTime = 0.0f;
    float lastFrameTime = 0.0f;

    Input input{};

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        float currentTime = glfwGetTime();
        deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        ProcessInput(window, input, io);

        if (input.w_pressed) camera.ProcessKeyboard(CAM_FORWARD, deltaTime);
        if (input.a_pressed) camera.ProcessKeyboard(CAM_LEFT, deltaTime);
        if (input.s_pressed) camera.ProcessKeyboard(CAM_BACKWARD, deltaTime);
        if (input.d_pressed) camera.ProcessKeyboard(CAM_RIGHT, deltaTime);

        if (input.left_mouse_pressed) camera.ProcessMouseMovement(input.mouse_delta_x, input.mouse_delta_y);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        static float model_y = 0.0f;
        AnimatedModel* model = nullptr;
        {
            static ImGuiComboFlags flags = 0;
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Animation Select");

            const int num_models = (int)models.size();
            auto model_names = std::make_unique<const char* []>(num_models);
            for (int i = 0; i < num_models; i++) model_names[i] = models[i].name.c_str();
            static int current_model_idx = 0;
            const char* combo_preview_value = model_names[current_model_idx];  // Pass in the preview value visible before opening the combo (it could be anything)
            model = &models[current_model_idx];
            if (ImGui::BeginCombo("ModelFile", combo_preview_value, flags))
            {
                for (int n = 0; n < num_models; n++)
                {
                    const bool is_selected = (current_model_idx == n);
                    if (ImGui::Selectable(model_names[n], is_selected))
                    {
                        if (current_model_idx != n)
                        {
                            model->current_clip = n;
                            model->clip_time = 0.0f;
                        }
                        current_model_idx = n;
                    }

                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                    if (is_selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            const int num_animations = (int)model->clips.size();
            auto animation_names = std::make_unique<const char* []>(num_animations);
            for (int i = 0; i < num_animations; i++) animation_names[i] = model->clips[i].name.c_str();
            combo_preview_value = animation_names[model->current_clip];  // Pass in the preview value visible before opening the combo (it could be anything)
            if (ImGui::BeginCombo("Animation", combo_preview_value, flags))
            {
                for (int n = 0; n < num_animations; n++)
                {
                    const bool is_selected = (model->current_clip == n);
                    if (ImGui::Selectable(animation_names[n], is_selected))
                    {
                        if (model->current_clip != n)
                        {
                            model->current_clip = n;
                            model->clip_time = 0.0f;
                        }
                        model->current_clip = n;
                    }

                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                    if (is_selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::Checkbox("Pause", &model->paused);
            ImGui::InputFloat("Speed", &model->clip_speed, 0.1f);
            ImGui::Text("Clip time: %f", model->clip_time);
            auto& clip = model->CurrentClip();
            if (ImGui::SliderFloat("Clip time", &model->clip_time, 0.0f, clip.frame_count / clip.frames_per_second))
            {
                model->paused = true;
            }
            ImGui::ProgressBar(model->clip_time / (clip.frame_count / clip.frames_per_second));
            ImGui::InputFloat("ModelFile y", &model_y);
            ImGui::Checkbox("Apply root motion", &model->apply_root_motion);
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

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

        glm::mat4 model_to_world = glm::identity<glm::mat4>();
        model_to_world = glm::translate(model_to_world, glm::vec3(0, 0, model_y));
        model_to_world = glm::scale(model_to_world, glm::vec3(0.01f, 0.01f, 0.01f));
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(view * model_to_world));
        shader.SetMat4("model", glm::value_ptr(model_to_world));
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

        model->Draw(shader, deltaTime);
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
