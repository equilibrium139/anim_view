#pragma once

#include "AnimatedModel.h"
#include <algorithm>
#include "Camera.h"
#include <glm/glm.hpp>
#include "Light.h"
#include "Input.h"
#include "Shader.h"
#include <string_view>
#include <vector>

class Scene
{
public:
	Scene(const std::vector<AnimatedModel>& models, unsigned int proj_view_ubo, unsigned int lights_ubo, Shader& shader) 
		: models(models), proj_view_ubo(proj_view_ubo), lights_ubo(lights_ubo), model_shader(&shader) {}
	void UpdateAndRender(const Input& input, float dt);
	virtual ~Scene() = default;

	static constexpr int max_point_lights = 25;
	static constexpr int max_spot_lights = 25;
protected:
	const std::vector<AnimatedModel>& models;
	Shader* model_shader;
	Camera camera{ glm::vec3{0.0f, 0.0f, 3.0f} };
	std::vector<PointLight> point_lights;
	std::vector<SpotLight> spot_lights;
	DirectionalLight dir_light{ glm::vec3(0.2f, 0.2f, 0.2f), glm::vec3(0.8, 0.8f, 0.8f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f) };
	unsigned int proj_view_ubo, lights_ubo;
private:
	virtual void UpdateAndRenderImpl(const Input& input, float dt) = 0;
};