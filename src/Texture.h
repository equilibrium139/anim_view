#ifndef TEXTURE_H
#define TEXTURE_H

#include <string>
#include <vector>

unsigned int LoadTexture(const char* fileName, const std::string& directory);
unsigned int LoadTexture(unsigned char* buffer, int length, const std::string& identifier);
unsigned int White1x1Texture();
// Use in place of missing normal map (blue = (0, 0, 1) so tangent and bitangent are non-factors
unsigned int Blue1x1Texture();
unsigned int LoadCubemap(const std::vector<std::string>& faces);

struct Texture
{
	unsigned int id;
};

#endif