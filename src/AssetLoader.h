//#pragma once
//
//#include <cstdint>
//#include <cstddef>
//#include <type_traits>
//#include <vector>
//#include <span>
//
//enum class VertexFlags : std::uint32_t
//{
//	DEFAULT = 0, // vec3 position vec3 normal vec2 uv
//	HAS_TANGENT = 1, // vec3 tangent
//	HAS_JOINT_DATA = 2, // uint32 joint indices vec4 joint weights
//};
//
//inline VertexFlags operator | (VertexFlags lhs, VertexFlags rhs)
//{
//	using T = std::underlying_type_t<VertexFlags>;
//	return (VertexFlags)((T)lhs | (T)rhs);
//}
//
//inline VertexFlags& operator |= (VertexFlags& lhs, VertexFlags rhs)
//{
//	lhs = lhs | rhs;
//	return lhs;
//}
//
//inline bool HasFlag(VertexFlags flags, VertexFlags flag_to_check)
//{
//	return (std::underlying_type_t<VertexFlags>)(flags | flag_to_check) != 0;
//}
//
//struct Mesh
//{
//	unsigned int indices_begin, indices_end;
//	unsigned int material_index;
//};
//
//struct PhongMaterialFile
//{
//
//};
//
//struct ModelFile
//{
//	struct Header
//	{
//		std::uint32_t magic_number;
//		std::uint32_t num_meshes;
//		std::uint32_t num_vertices;
//		std::uint32_t num_indices;
//		std::uint32_t num_materials;
//		VertexFlags vertex_flags = VertexFlags::DEFAULT;
//		// add padding if needed
//	};
//	std::vector<std::byte> file_contents;
//	Header* header;
//	std::span<Mesh> meshes;
//	std::span<std::byte> vertex_buffer;
//	std::span<unsigned int> index_buffer;
//	std::unique_ptr<Mesh[]> meshes;
//	std::unique_ptr<std::uint8_t[]> vertex_buffer;
//	std::unique_ptr<unsigned int[]> indices;
//	std::unique_ptr<PhongMaterial[]> materials;
//	std::string name;
//};
//
//struct SkeletonFile
//{
//	struct Header
//	{
//		std::uint32_t magic_number;
//		std::uint32_t num_joints;
//	};
//	Header header;
//	std::unique_ptr<Joint[]> joints;
//	std::unique_ptr<std::string[]> joint_names;
//};
//
//struct AnimationClipFile
//{
//	struct Header
//	{
//		using bool32 = std::uint32_t; // for padding purposes
//		std::uint32_t magic_number;
//		std::uint32_t frame_count;
//		float frames_per_second;
//		bool32 loops = false; // fix later
//		// add padding if needed
//	};
//	Header header;
//	std::unique_ptr<SkeletonPose[]> skeleton_poses; // number of poses = frame_count + 1 or frame_count if loops
//	std::string name;
//};
