#pragma once

#include "Scene.h"

class PoseEditScene : public Scene
{
public:
	PoseEditScene(const std::vector<AnimatedModel>& models, unsigned int prov_view_ubo, unsigned int lights_ubo, Shader& model_shader);

private:
	virtual void UpdateAndRenderImpl(const Input& input, float dt) override;

	struct ModelState
	{
		SkeletonPose pose;
		glm::vec3 position = { 0.0f, 0.0f, 0.0f };
		glm::vec3 scale = { 0.01f, 0.01f, 0.01f };
	};

	std::vector<ModelState> model_states;
	int current_model_idx = 0;
};