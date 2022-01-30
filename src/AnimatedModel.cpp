#include "AnimatedModel.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <utility>
#include <glm/glm.hpp>
#include "stb_image.h"

#include <glm/ext/matrix_relational.hpp>

AnimatedModel::AnimatedModel(const std::string& directory)
{
	LoadAnimatedModel(directory);

	// Render opaque meshes before transparent ones
	std::partition(meshes.begin(), meshes.end(),
		[&materials = this->materials](const Mesh& mesh)
	{
		auto& material = materials[mesh.material_index];
		return !material.HasFlag(PhongMaterialFlags::DIFFUSE_WITH_ALPHA);
	});
}

static inline glm::vec3 Lerp(const glm::vec3& a, const glm::vec3& b, float t)
{
	return a * (1.0f - t) + b * t;
}

static SkeletonPose Interpolate(const SkeletonPose& a, const SkeletonPose& b, float t)
{
	const auto num_poses = a.joint_poses.size();
	SkeletonPose interpolated;
	interpolated.joint_poses.resize(num_poses);
	for (auto i = 0u; i < num_poses; i++)
	{
		auto& a_pose = a.joint_poses[i];
		auto& b_pose = b.joint_poses[i];
		auto& interpolated_joint_pose = interpolated.joint_poses[i];
		interpolated_joint_pose.translation = Lerp(a_pose.translation, b_pose.translation, t);
		interpolated_joint_pose.scale = Lerp(a_pose.scale, b_pose.scale, t);
		// Some animations have weird stutters with quaternion lerp (Warrok wave dance for example)
		interpolated_joint_pose.rotation = glm::slerp(a_pose.rotation, b_pose.rotation, t);
	}
	return interpolated;
}

void AnimatedModel::LoadAnimatedModel(const std::string& directory)
{
	namespace fs = std::filesystem;

	const auto path = fs::path(directory);
	assert(fs::is_directory(path));
	fs::path model_file_path, skeleton_file_path;
	std::vector<fs::path> animation_file_paths;
	for (const auto& dir_entry : fs::directory_iterator(path))
	{
		auto extension = dir_entry.path().extension();
		if (extension == ".model")
		{
			assert(model_file_path.empty());
			model_file_path = dir_entry.path();
		}
		else if (extension == ".skeleton")
		{
			assert(skeleton_file_path.empty());
			skeleton_file_path = dir_entry.path();
		}
		else if (extension == ".animation")
		{
			animation_file_paths.push_back(dir_entry.path());
		}
		else if (extension != ".png" && extension != ".jpg")
		{
			std::cout << "Warning: unsupported file format: '" << extension << "'\n";
		}
	}

	assert(!model_file_path.empty() && !skeleton_file_path.empty());

	this->name = model_file_path.stem().string();

	std::ifstream model_file_stream(model_file_path, std::ios::binary);

	ModelFile model_file_data;
	model_file_stream.read((char*)&model_file_data.header, sizeof(model_file_data.header));
	assert(model_file_data.header.magic_number == 'ldom');
	model_file_data.meshes = std::make_unique<Mesh[]>(model_file_data.header.num_meshes);
	model_file_stream.read((char*)model_file_data.meshes.get(), model_file_data.header.num_meshes * sizeof(Mesh));
	const auto has_tangents = HasFlag(model_file_data.header.vertex_flags, VertexFlags::HAS_TANGENT);
	const auto has_joint_data = HasFlag(model_file_data.header.vertex_flags, VertexFlags::HAS_JOINT_DATA);
	constexpr auto default_vertex_size_bytes = sizeof(glm::vec3) + sizeof(glm::vec3) + sizeof(glm::vec2); // position normal uv
	constexpr auto tangent_data_size_bytes = sizeof(glm::vec3);
	constexpr auto joint_data_size_bytes = sizeof(std::uint32_t) + sizeof(glm::vec4);
	const auto vertex_size_bytes = default_vertex_size_bytes + (has_tangents ? tangent_data_size_bytes : 0) + (has_joint_data ? joint_data_size_bytes : 0);
	const auto vertex_buffer_size_bytes = model_file_data.header.num_vertices * vertex_size_bytes;
	model_file_data.vertex_buffer = std::make_unique<std::uint8_t[]>(vertex_buffer_size_bytes);
	model_file_data.indices = std::make_unique<unsigned int[]>(model_file_data.header.num_indices);
	model_file_data.materials = std::make_unique<PhongMaterial[]>(model_file_data.header.num_materials);
	model_file_stream.read((char*)model_file_data.vertex_buffer.get(), vertex_buffer_size_bytes);
	const auto index_buffer_size_bytes = model_file_data.header.num_indices * sizeof(unsigned int);
	model_file_stream.read((char*)model_file_data.indices.get(), index_buffer_size_bytes);
	for (auto i = 0u; i < model_file_data.header.num_materials; i++)
	{
		auto& material = model_file_data.materials[i];
		model_file_stream.read((char*)&material.diffuse_coefficient, sizeof(material.diffuse_coefficient));
		model_file_stream.read((char*)&material.specular_coefficient, sizeof(material.specular_coefficient));
		model_file_stream.read((char*)&material.shininess, sizeof(material.shininess));
		model_file_stream.read((char*)&material.flags, sizeof(material.flags));
		std::string diffusemap_filename; std::getline(model_file_stream, diffusemap_filename, '\0');
		std::string specularmap_filename; std::getline(model_file_stream, specularmap_filename, '\0');
		std::string normalmap_filename; std::getline(model_file_stream, normalmap_filename, '\0');
		if (!diffusemap_filename.empty()) material.diffuse_map.id = LoadTexture(diffusemap_filename.c_str(), directory);
		else material.diffuse_map.id = White1x1Texture();
		if (!specularmap_filename.empty()) material.specular_map.id = LoadTexture(specularmap_filename.c_str(), directory);
		else material.specular_map.id = White1x1Texture();
		if (!normalmap_filename.empty()) material.normal_map.id = LoadTexture(normalmap_filename.c_str(), directory);
		else material.normal_map.id = Blue1x1Texture();
	}

	std::copy(model_file_data.meshes.get(), model_file_data.meshes.get() + model_file_data.header.num_meshes, std::back_inserter(this->meshes));
	std::copy(model_file_data.materials.get(), model_file_data.materials.get() + model_file_data.header.num_materials, std::back_inserter(this->materials));

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertex_buffer_size_bytes, model_file_data.vertex_buffer.get(), GL_STATIC_DRAW);
	
	glGenBuffers(1, &EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * model_file_data.header.num_indices, model_file_data.indices.get(), GL_STATIC_DRAW);

	const char* offset = 0;
	std::uint32_t attribute_index = 0;

	// default vertex data- every vertex buffer has at least position normal uv
	// position
	auto stride = (GLsizei)vertex_size_bytes;
	glVertexAttribPointer(attribute_index, 3, GL_FLOAT, GL_FALSE, stride, offset);
	glEnableVertexAttribArray(attribute_index);
	offset += sizeof(glm::vec3);
	attribute_index++;
	
	// normal
	glVertexAttribPointer(attribute_index, 3, GL_FLOAT, GL_FALSE, stride, offset);
	glEnableVertexAttribArray(attribute_index);
	offset += sizeof(glm::vec3);
	attribute_index++;

	// uv
	glVertexAttribPointer(attribute_index, 2, GL_FLOAT, GL_FALSE, stride, offset);
	glEnableVertexAttribArray(attribute_index);
	offset += sizeof(glm::vec2);
	attribute_index++;

	if (has_tangents)
	{
		// tangent
		glVertexAttribPointer(attribute_index, 3, GL_FLOAT, GL_FALSE, stride, offset);
		glEnableVertexAttribArray(attribute_index);
		offset += sizeof(glm::vec3);
		attribute_index++;
	}
	if (has_joint_data)
	{
		// joint indices
		glVertexAttribIPointer(attribute_index, 1, GL_UNSIGNED_INT, stride, offset);
		glEnableVertexAttribArray(attribute_index);
		offset += sizeof(std::uint32_t);
		attribute_index++;

		// joint weights
		glVertexAttribPointer(attribute_index, 4, GL_FLOAT, GL_FALSE, stride, offset);
		glEnableVertexAttribArray(attribute_index);
		offset += sizeof(glm::vec4);
		attribute_index++;
	}

	std::ifstream skeleton_file_stream(skeleton_file_path, std::ios::binary);
	SkeletonFile skeleton_file_data;
	skeleton_file_stream.read((char*)&skeleton_file_data.header, sizeof(skeleton_file_data.header));
	assert(skeleton_file_data.header.magic_number == 'ntks');
	skeleton_file_data.joints = std::make_unique<Joint[]>(skeleton_file_data.header.num_joints);
	skeleton_file_data.joint_names = std::make_unique<std::string[]>(skeleton_file_data.header.num_joints);
	skeleton_file_stream.read((char*)skeleton_file_data.joints.get(), skeleton_file_data.header.num_joints * sizeof(Joint));
	for (auto i = 0u; i < skeleton_file_data.header.num_joints; i++)
	{
		auto& joint_name = skeleton_file_data.joint_names[i];
		std::getline(skeleton_file_stream, joint_name, '\0');
	}

	std::copy(skeleton_file_data.joints.get(), skeleton_file_data.joints.get() + skeleton_file_data.header.num_joints, std::back_inserter(this->skeleton.joints));
	std::copy(skeleton_file_data.joint_names.get(), skeleton_file_data.joint_names.get() + skeleton_file_data.header.num_joints, std::back_inserter(this->skeleton.joint_names));

	auto AddAnimation = [&clips = this->clips](const fs::path& path, int num_skeleton_joints)
	{
		std::ifstream animation_file_stream(path, std::ios::binary);
		AnimationClipFile::Header clip_file_header;
		animation_file_stream.read((char*)&clip_file_header, sizeof(clip_file_header));
		clips.emplace_back();
		auto& new_clip = clips.back();
		new_clip.frames_per_second = clip_file_header.frames_per_second;
		new_clip.frame_count = clip_file_header.frame_count;
		new_clip.loops = clip_file_header.loops;
		new_clip.name = path.stem().string();
		const auto num_poses = clip_file_header.frame_count + (clip_file_header.loops ? 0 : 1);
		new_clip.poses.resize(num_poses);
		const auto pose_size_bytes = num_skeleton_joints * sizeof(JointPose);
		for (auto& skeleton_pose : new_clip.poses)
		{
			skeleton_pose.joint_poses.resize(num_skeleton_joints);
			animation_file_stream.read((char*)skeleton_pose.joint_poses.data(), pose_size_bytes);

			// Quaternions are stored in the file in the order w, x, y, z. GLM stores them in the order
			// x, y, z, w even though the glm::quat constructor takes them in the order w, x, y, z. This is fixing
			// that ordering issue
			for (auto& pose : skeleton_pose.joint_poses) {
				pose.rotation = glm::quat(pose.rotation.x, pose.rotation.y, pose.rotation.z, pose.rotation.w);
			}
		}
	};

	for (auto& animation_file_path : animation_file_paths)
	{
		AddAnimation(animation_file_path, (int)skeleton.joints.size());
	}
}

std::vector<glm::mat4> ComputeGlobalMatrices(const SkeletonPose& pose, const Skeleton& skeleton, bool apply_root_motion)
{
	assert(pose.joint_poses.size() == skeleton.joints.size());

	glm::mat4 local_mat = glm::identity<glm::mat4>();
	if (apply_root_motion) local_mat = glm::translate(local_mat, pose.joint_poses[0].translation);
	else local_mat = glm::translate(local_mat, glm::vec3(pose.joint_poses[0].translation.x, pose.joint_poses[0].translation.y, 0));
	local_mat *= glm::mat4_cast(pose.joint_poses[0].rotation);
	local_mat = glm::scale(local_mat, pose.joint_poses[0].scale);

	std::vector<glm::mat4> global_joint_poses(pose.joint_poses.size());
	global_joint_poses[0] = local_mat;

	for (int i = 1; i < global_joint_poses.size(); i++)
	{
		auto& joint = skeleton.joints[i];
		auto& parent_global = global_joint_poses[joint.parent];

		local_mat = glm::identity<glm::mat4>();
		local_mat = glm::translate(local_mat, pose.joint_poses[i].translation);
		local_mat *= glm::mat4_cast(pose.joint_poses[i].rotation);
		local_mat = glm::scale(local_mat, pose.joint_poses[i].scale);
		global_joint_poses[i] = parent_global * local_mat;
	}

	return global_joint_poses;
}

std::vector<glm::mat4> ComputeGlobalMatrices(const AnimationClip& clip, const Skeleton& skeleton, float clip_time, bool apply_root_motion)
{
	float pose_index = clip_time * clip.frames_per_second;
	auto a = (int)std::floor(pose_index);
	auto b = a + 1;
	auto pose = Interpolate(clip.poses[a], clip.poses[b], pose_index - a);

	return ComputeGlobalMatrices(pose, skeleton, apply_root_motion);
}

std::vector<glm::mat4> ComputeSkinningMatrices(const AnimationClip& clip, const Skeleton& skeleton, float clip_time, bool apply_root_motion)
{
	auto global_joint_poses = ComputeGlobalMatrices(clip, skeleton, clip_time, apply_root_motion);

	std::vector<glm::mat4> skinning_matrices(global_joint_poses.size());
	for (int i = 0; i < global_joint_poses.size(); i++)
	{
		skinning_matrices[i] = global_joint_poses[i] * glm::mat4(skeleton.joints[i].local_to_joint);
	}

	return skinning_matrices;
}

std::vector<glm::mat4> ComputeSkinningMatrices(const SkeletonPose& pose, const Skeleton& skeleton, bool apply_root_motion)
{
	auto global_joint_poses = ComputeGlobalMatrices(pose, skeleton, apply_root_motion);

	std::vector<glm::mat4> skinning_matrices(global_joint_poses.size());
	for (int i = 0; i < global_joint_poses.size(); i++)
	{
		skinning_matrices[i] = global_joint_poses[i] * glm::mat4(skeleton.joints[i].local_to_joint);
	}

	return skinning_matrices;
}

static JointPose ToJointPose(const glm::mat4& mat)
{
	JointPose pose;
	pose.rotation = glm::quat(mat);
	pose.translation = mat[3];
	pose.scale = { mat[0][0], mat[1][1], mat[2][2] };
	return pose;
}

SkeletonPose ComputeLocalMatrices(const std::vector<glm::mat4>& global_matrices, const Skeleton& skeleton)
{
	SkeletonPose pose;
	auto num_joints = global_matrices.size();
	pose.joint_poses.resize(num_joints);

	pose.joint_poses[0] = ToJointPose(global_matrices[0]);

	for (int i = 1; i < num_joints; i++)
	{
		auto parent_index = skeleton.joints[i].parent;
		auto parent_global_inverse = glm::inverse(global_matrices[parent_index]);
		auto local_mat = parent_global_inverse * global_matrices[i];
		pose.joint_poses[i] = ToJointPose(local_mat);
	}

	return pose;
}
