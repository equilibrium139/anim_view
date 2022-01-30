#include "ClipPickScene.h"

#include "imgui.h"

ClipPickScene::ClipPickScene(const std::vector<AnimatedModel>& models, unsigned int proj_view_ubo, unsigned int lights_ubo, Shader& shader)
    :Scene(models, proj_view_ubo, lights_ubo, shader), model_states(models.size()), model_names(models.size())
{
    const int num_models = (int)models.size();
    for (int i = 0; i < num_models; i++)
    {
        model_names[i] = models[i].name;

        std::transform(models[i].clips.begin(), models[i].clips.end(), std::back_inserter(model_states[i].clip_names),
            [](const AnimationClip& clip)
            {
                return clip.name;
            });
    }
}

void ClipPickScene::UpdateAndRenderImpl(const Input& input, float dt)
{
    if (input.w_pressed) camera.ProcessKeyboard(CAM_FORWARD, dt);
    if (input.a_pressed) camera.ProcessKeyboard(CAM_LEFT, dt);
    if (input.s_pressed) camera.ProcessKeyboard(CAM_BACKWARD, dt);
    if (input.d_pressed) camera.ProcessKeyboard(CAM_RIGHT, dt);
    if (input.left_mouse_pressed) camera.ProcessMouseMovement(input.mouse_delta_x, input.mouse_delta_y);

    {
        static constexpr ImGuiComboFlags flags = 0;

        ImGui::Begin("Animation Select");

        const int num_models = (int)models.size();
        auto& model_combo_preview_value = model_names[current_model_idx];
        if (ImGui::BeginCombo("Model", model_combo_preview_value.c_str(), flags))
        {
            for (int n = 0; n < num_models; n++)
            {
                const bool is_selected = (current_model_idx == n);
                if (ImGui::Selectable(model_names[n].c_str(), is_selected))
                {
                    if (current_model_idx != n)
                    {
                        model_states[current_model_idx].clip_time = 0.0f;
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

        const int num_animations = (int)model_states[current_model_idx].clip_names.size();
        auto& clip_combo_preview_value = model_states[current_model_idx].clip_names[model_states[current_model_idx].current_clip];
        if (ImGui::BeginCombo("Animation", clip_combo_preview_value.c_str(), flags))
        {
            for (int n = 0; n < num_animations; n++)
            {
                const bool is_selected = (model_states[current_model_idx].current_clip == n);
                if (ImGui::Selectable(model_states[current_model_idx].clip_names[n].c_str(), is_selected))
                {
                    if (model_states[current_model_idx].current_clip != n)
                    {
                        model_states[current_model_idx].clip_time = 0.0f;
                    }
                    model_states[current_model_idx].current_clip = n;
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Checkbox("Pause", &model_states[current_model_idx].paused);
        ImGui::InputFloat("Speed", &model_states[current_model_idx].clip_speed, 0.1f);
        ImGui::Text("Clip time: %f", model_states[current_model_idx].clip_time);

        auto& current_clip = models[current_model_idx].clips[model_states[current_model_idx].current_clip];
        if (ImGui::SliderFloat("Clip time", &model_states[current_model_idx].clip_time, 0.0f, current_clip.frame_count / current_clip.frames_per_second))
        {
            model_states[current_model_idx].paused = true;
        }
        ImGui::ProgressBar(model_states[current_model_idx].clip_time / (current_clip.frame_count / current_clip.frames_per_second));
        ImGui::InputFloat3("Position", &model_states[current_model_idx].position.x);
        ImGui::DragFloat("Scale", &model_states[current_model_idx].scale, 0.001f);
        ImGui::Checkbox("Apply root motion", &model_states[current_model_idx].apply_root_motion);
        ImGui::Checkbox("Render skeleton", &model_states[current_model_idx].render_skeleton);
        ImGui::Checkbox("Render model", &model_states[current_model_idx].render_model);
        ImGui::DragFloat("Axis scale", &model_states[current_model_idx].axis_scale, 0.01f);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

        ImGui::End();
    }

    const auto& current_model = models[current_model_idx];
    auto& current_model_state = model_states[current_model_idx];
    const auto& current_clip = current_model.clips[current_model_state.current_clip];

    if (!current_model_state.paused)
    {
        current_model_state.clip_time += (current_model_state.clip_speed * dt);
        const float clip_duration = current_clip.frame_count / current_clip.frames_per_second;
        current_model_state.clip_time = std::fmod(current_model_state.clip_time, clip_duration);
        if (current_model_state.clip_time < 0) current_model_state.clip_time = clip_duration + current_model_state.clip_time;
    }

    if (current_model_state.render_model)
    {
        auto view_matrix = camera.GetViewMatrix();
        auto world_matrix = glm::identity<glm::mat4>();
        world_matrix = glm::translate(world_matrix, current_model_state.position);
        world_matrix = glm::scale(world_matrix, glm::vec3(current_model_state.scale));
        auto normal_matrix = glm::mat3(glm::transpose(glm::inverse(view_matrix * world_matrix)));

        current_model.BindGeometry();
        model_shader->use();
        auto skinning_matrices = ComputeSkinningMatrices(current_clip, current_model.skeleton, current_model_state.clip_time, current_model_state.apply_root_motion);
        model_shader->SetMat4("skinning_matrices", glm::value_ptr(skinning_matrices.front()), (int)skinning_matrices.size());
        model_shader->SetMat4("model", glm::value_ptr(world_matrix));
        model_shader->SetMat3("normalMatrix", glm::value_ptr(normal_matrix));

        const auto& materials = current_model.materials;
        for (auto& mesh : current_model.meshes)
        {
            auto& material = materials[mesh.material_index];
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, material.diffuse_map.id);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, material.specular_map.id);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, material.normal_map.id);
            model_shader->SetInt("material.diffuse", 0);
            model_shader->SetInt("material.specular", 1);
            model_shader->SetInt("material.normal", 2);
            model_shader->SetFloat("material.shininess", material.shininess);
            model_shader->SetVec3("material.diffuse_coeff", material.diffuse_coefficient);
            model_shader->SetVec3("material.specular_coeff", material.specular_coefficient);
            static_assert(std::is_same_v<std::uint32_t, std::underlying_type<PhongMaterialFlags>::type>);
            model_shader->SetUint("material.flags", (std::uint32_t)material.flags);
            glDrawElements(GL_TRIANGLES, mesh.indices_end - mesh.indices_begin + 1, GL_UNSIGNED_INT, (void*)(mesh.indices_begin * sizeof(GLuint)));
        }
    }
    if (current_model_state.render_skeleton)
    {
        auto& axis = GetAxis();
        glBindVertexArray(axis.vao);
        auto& axis_shader = axis.shader;
        axis_shader.use();
        auto world_matrix = glm::identity<glm::mat4>();
        world_matrix = glm::translate(world_matrix, current_model_state.position);
        world_matrix = glm::scale(world_matrix, glm::vec3(current_model_state.scale));
        static constexpr glm::vec3 red(1.0f, 0.0f, 0.0f);
        static constexpr glm::vec3 green(0.0f, 1.0f, 0.0f);
        static constexpr glm::vec3 blue(0.0f, 0.0f, 1.0f);

        auto global_matrices = ComputeGlobalMatrices(current_clip, current_model.skeleton, current_model_state.clip_time, current_model_state.apply_root_motion);
        auto scale_matrix = glm::scale(glm::identity<glm::mat4>(), glm::vec3(current_model_state.axis_scale));
        glDisable(GL_DEPTH_TEST);
        for (auto& mat : global_matrices)
        {
            auto joint_world_matrix = world_matrix * mat * scale_matrix;
            axis_shader.SetMat4("model", glm::value_ptr(joint_world_matrix));
            axis_shader.SetVec3("color", red);
            glDrawArrays(GL_LINES, 0, 2);
            axis_shader.SetVec3("color", green);
            glDrawArrays(GL_LINES, 2, 2);
            axis_shader.SetVec3("color", blue);
            glDrawArrays(GL_LINES, 4, 2);
        }
        glEnable(GL_DEPTH_TEST);
    }
}

ClipPickScene::Axis::Axis()
{
    static float vertices[] =
    {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
    };

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, 0);
    glEnableVertexAttribArray(0);
}
