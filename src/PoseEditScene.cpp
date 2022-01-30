#include "PoseEditScene.h"

#include <algorithm>
#include "glm/glm.hpp"
#include "imgui.h"

PoseEditScene::PoseEditScene(const std::vector<AnimatedModel>& models, unsigned int proj_view_ubo, unsigned int lights_ubo, Shader& model_shader)
	:Scene(models, proj_view_ubo, lights_ubo, model_shader), model_states(models.size())
{
	// Resize skeleton pose to have same size as the number of joints in the skeleton it belongs to
	int num_models = (int)models.size();
	for (int i = 0; i < num_models; i++)
	{
		const auto& model = models[i];
		auto& model_state = model_states[i];

		/*static constexpr JointPose identity_pose = {
			.rotation = glm::identity<glm::quat>(),
			.translation = glm::vec3(0.0f, 0.0f, 0.0f),
			.scale = glm::vec3(1.0f, 1.0f, 1.0f),
		};*/

		const auto num_joints = model.skeleton.joints.size();
		std::vector<glm::mat4> bind_pose_global_mats(num_joints);
		std::transform(model.skeleton.joints.begin(), model.skeleton.joints.end(), bind_pose_global_mats.begin(),
			[](const Joint& joint)
			{
				return glm::inverse(glm::mat4(joint.local_to_joint));
			});

		model_state.pose = ComputeLocalMatrices(bind_pose_global_mats, model.skeleton);

		/*model_state.pose.joint_poses.resize(model.skeleton.joints.size());

		for (int j = 0; j < model.skeleton.joints.size(); j++)
		{
			auto mat = glm::inverse(glm::mat4(model.skeleton.joints[j].local_to_joint));
			model_state.pose.joint_poses[j].rotation = glm::quat(mat);
			model_state.pose.joint_poses[j].translation = glm::vec3(-model.skeleton.joints[j].local_to_joint[3]);
			model_state.pose.joint_poses[j].scale = glm::vec3(mat[0][0], mat[1][1], mat[2][2]);
		}*/

		/*std::vector<glm::mat4> inverses(model.skeleton.joints.size());
		for (int i = 0; i < (int)inverses.size(); i++)
		{
			inverses[i]
		}*/
	}
}

void PoseEditScene::UpdateAndRenderImpl(const Input& input, float dt)
{
	if (input.w_pressed) camera.ProcessKeyboard(CAM_FORWARD, dt);
	if (input.a_pressed) camera.ProcessKeyboard(CAM_LEFT, dt);
	if (input.s_pressed) camera.ProcessKeyboard(CAM_BACKWARD, dt);
	if (input.d_pressed) camera.ProcessKeyboard(CAM_RIGHT, dt);
	if (input.left_mouse_pressed) camera.ProcessMouseMovement(input.mouse_delta_x, input.mouse_delta_y);

    {
		static constexpr ImGuiComboFlags flags = 0;

		ImGui::Begin("Pose Edit");

		const int num_models = (int)models.size();
		auto& model_combo_preview_value = models[current_model_idx].name;
		if (ImGui::BeginCombo("Model", model_combo_preview_value.c_str(), flags))
		{
			for (int n = 0; n < num_models; n++)
			{
				const bool is_selected = (current_model_idx == n);
				if (ImGui::Selectable(models[n].name.c_str(), is_selected))
				{
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

		const auto& current_model = models[current_model_idx];
		const auto num_joints = (int)current_model.skeleton.joints.size();
		for (int i = 0; i < num_joints; i++)
		{
			const auto& name = current_model.skeleton.joint_names[i];
			if (ImGui::TreeNode(name.c_str()))
			{
				ImGui::DragFloat3("Scale", &model_states[current_model_idx].pose.joint_poses[i].scale.x, 0.01f, 0.0f, 10.0f);
				ImGui::DragFloat3("Translation", &model_states[current_model_idx].pose.joint_poses[i].translation.x, 0.1f, -1000.0f, 1000.0f);
				glm::mat3 rotation{ model_states[current_model_idx].pose.joint_poses[i].rotation };
				ImGui::DragFloat3("Rotation X", &rotation[0][0], 0.1f, -36000.0f, 36000.0f);
				ImGui::DragFloat3("Rotation Y", &rotation[1][0], 0.1f, -36000.0f, 36000.0f);
				ImGui::DragFloat3("Rotation X", &rotation[2][0], 0.1f, -36000.0f, 36000.0f);
				model_states[current_model_idx].pose.joint_poses[i].rotation = rotation;
				ImGui::TreePop();
			}
		}

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

		ImGui::End();
	}

	const auto& current_model = models[current_model_idx];
	auto& current_model_state = model_states[current_model_idx];

	auto view_matrix = camera.GetViewMatrix();
	auto world_matrix = glm::identity<glm::mat4>();
	world_matrix = glm::translate(world_matrix, current_model_state.position);
	world_matrix = glm::scale(world_matrix, glm::vec3(current_model_state.scale));
	auto normal_matrix = glm::mat3(glm::transpose(glm::inverse(view_matrix * world_matrix)));

	current_model.BindGeometry();
	model_shader->use();
	//auto skinning_matrices = ComputeSkinningMatrices(current_model_state.pose, current_model.skeleton);
	//auto skinning_matrices = current_model_state.pose.joint_poses;
	//std::vector<glm::mat4> skinning_matrices(100, glm::identity<glm::mat4>());
	auto skinning_matrices = ComputeSkinningMatrices(current_model_state.pose, current_model.skeleton);
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
