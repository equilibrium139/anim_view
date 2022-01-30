#include "Scene.h"

#include <algorithm>
#include <vector>

class ClipPickScene : public Scene
{
public:
	ClipPickScene(const std::vector<AnimatedModel>& models, unsigned int proj_view_ubo, unsigned int lights_ubo, Shader& model_shader);

private:
	virtual void UpdateAndRenderImpl(const Input& input, float dt) override;

	struct ModelState
	{
		std::vector<std::string> clip_names;
		glm::vec3 position = { 0.0f, -1.0f, 0.0f };
		float scale = 0.01f; // Mixamo models are using cm so converting to m
		float axis_scale = 10.0f;
		int current_clip = 0;
		float clip_speed = 1.0f;
		float clip_time = 0.0f;
		bool paused = false;
		bool apply_root_motion = false;
		bool render_skeleton = false;
		bool render_model = true;
	};

	struct Axis
	{
		Shader shader{ "Shaders/axis.vert", "Shaders/axis.frag", nullptr,
			{
				{
					.uniform_block_name = "Matrices",
					.uniform_block_binding = 0
				}
			}
		};

		unsigned int vao, vbo;

		Axis();
	};

	std::vector<ModelState> model_states;
	std::vector<std::string> model_names;
	int current_model_idx = 0;
	static Axis& GetAxis()
	{
		static Axis axis;
		return axis;
	}
};
