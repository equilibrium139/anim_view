#include "Scene.h"
#include "imgui.h"

void Scene::UpdateAndRender(const Input& input, float dt)
{
    auto view = camera.GetViewMatrix();
    auto aspect = (float)input.window_width / (float)input.window_height;
    auto proj = camera.GetProjectionMatrix(aspect);

    glBindBuffer(GL_UNIFORM_BUFFER, proj_view_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4),
        glm::value_ptr(proj));
    glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4),
        glm::value_ptr(view));

    DirectionalLight dirLightViewSpace = DirectionalLight(glm::vec3(0.2f, 0.2f, 0.2f), glm::vec3(0.8, 0.8f, 0.8f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    const int num_lights[2] = { (int)point_lights.size(), (int)spot_lights.size() };
    glBindBuffer(GL_UNIFORM_BUFFER, lights_ubo);
    if (num_lights[0] > 0) glBufferSubData(GL_UNIFORM_BUFFER, 0, num_lights[0] * sizeof(PointLight), point_lights.data());
    if (num_lights[1] > 0) glBufferSubData(GL_UNIFORM_BUFFER, max_point_lights * sizeof(PointLight), num_lights[1] * sizeof(SpotLight), spot_lights.data());
    glBufferSubData(GL_UNIFORM_BUFFER, max_point_lights * sizeof(PointLight) + max_spot_lights * sizeof(SpotLight) + sizeof(DirectionalLight), sizeof(num_lights), num_lights);
    glBufferSubData(GL_UNIFORM_BUFFER, max_point_lights * sizeof(PointLight) + max_spot_lights * sizeof(SpotLight), sizeof(DirectionalLight), &dirLightViewSpace);

    UpdateAndRenderImpl(input, dt);
}
