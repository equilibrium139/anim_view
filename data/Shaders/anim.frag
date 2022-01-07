#version 330 core

out vec4 color;

struct PointLight {
	vec3 ambient;
    float range;
	vec3 diffuse;
	vec3 specular;
	vec3 position;
};

struct DirectionalLight {
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
	vec3 direction;
};

struct SpotLight {
	vec3 ambient;
    float rMaxSquared;
	vec3 diffuse;
    float cosineInnerCutoff;
	vec3 specular;
    float cosineOuterCutoff;
	vec3 position;
	vec3 direction;
};

#define MAX_NUM_POINT_LIGHTS 25
#define MAX_NUM_SPOT_LIGHTS 25

layout (std140) uniform Lights {
    PointLight pointLight[MAX_NUM_POINT_LIGHTS];
    SpotLight spotLight[MAX_NUM_SPOT_LIGHTS];
    DirectionalLight dirLight;
    int numPointLights;
    int numSpotLights;
};

#define MATERIAL_FLAG_DIFFUSE_WITH_ALPHA 1u

struct Material {
    sampler2D diffuse;  
    sampler2D specular;
    sampler2D normal;
    vec3 diffuse_coeff;
    vec3 specular_coeff;
    float shininess;
    uint flags;
};

uniform Material material;

in VS_OUT {
    mat3 TBN;
    vec3 fragViewPos;
    vec2 texCoords;
} fs_in;

vec3 CalcSpotLightContribution(SpotLight light, vec3 mat_ambient, vec3 mat_diffuse, vec3 mat_specular, vec3 normal, vec3 fragPos)
{
    vec3 L = light.position - fragPos;
    float distanceToLightSquared = dot(L, L);
    L = normalize(L);
    float distanceFalloffFactor = pow(max((1 - (distanceToLightSquared / light.rMaxSquared)), 0.0), 2.0);
    // float distanceFalloffFactor = (light.r0Squared / max(distanceToLightSquared, light.rMin));

    float fragAlignmentToLightDir = dot(-L, light.direction);
    float directionalFalloffFactor = pow(clamp((fragAlignmentToLightDir - light.cosineOuterCutoff) / (light.cosineInnerCutoff - light.cosineOuterCutoff), 0.0, 1.0), 2.0); 
    float totalAttenuation = distanceFalloffFactor * directionalFalloffFactor;

    vec3 ambient = light.ambient * mat_ambient;

    float diffuse_strength = max(dot(normal, L), 0.0);
    vec3 diffuse = diffuse_strength * light.diffuse * mat_diffuse;

    vec3 R = reflect(-L, normal);
    vec3 V = normalize(-fragPos);
    float specular_strength = max(dot(R, V), 0.0);
    vec3 specular = pow(specular_strength, material.shininess) * light.specular * mat_specular;

    return ambient + totalAttenuation * (diffuse + specular);
}

vec3 CalcDirLightContribution(DirectionalLight light, vec3 mat_ambient, vec3 mat_diffuse, vec3 mat_specular, vec3 normal, vec3 fragPos)
{
    vec3 L = -light.direction;

    vec3 ambient = light.ambient * mat_ambient;

    float diffuse_strength = max(dot(normal, L), 0.0);
    vec3 diffuse = diffuse_strength * light.diffuse * mat_diffuse;

    vec3 R = reflect(-L, normal);
    vec3 V = normalize(-fragPos);
    float specular_strength = max(dot(R, V), 0.0);
    vec3 specular = pow(specular_strength, material.shininess) * light.specular * mat_specular;

    return ambient + diffuse + specular;
}

void main()
{
    vec3 diffuse_texel;
    float alpha = 1.0;

    if ((material.flags & MATERIAL_FLAG_DIFFUSE_WITH_ALPHA) != 0u)
    {
        vec4 diffuse_texel_with_alpha = texture(material.diffuse, fs_in.texCoords).rgba;
        diffuse_texel = diffuse_texel_with_alpha.rgb;
        alpha = diffuse_texel_with_alpha.a;
    }
    else
    {
        diffuse_texel = texture(material.diffuse, fs_in.texCoords).rgb;
    }

    vec3 specular_texel = texture(material.specular, fs_in.texCoords).rgb;
    vec3 mat_diffuse = material.diffuse_coeff * diffuse_texel;
    vec3 mat_ambient = vec3(0.2, 0.2, 0.2) * mat_diffuse;
    vec3 mat_specular = material.specular_coeff * specular_texel;
    vec3 normal = texture(material.normal, fs_in.texCoords).rgb;
    normal = normal * 2.0 - 1.0;
    normal = normalize(fs_in.TBN * normal);

    // vec3 normal = normalize(fs_in.TBN[0]);

    vec3 finalColor = vec3(0.0, 0.0, 0.0);

    for (int i = 0; i < numSpotLights; i++)
    {
        finalColor += CalcSpotLightContribution(spotLight[i], mat_ambient, mat_diffuse, mat_specular, normal, fs_in.fragViewPos);
    }

    finalColor += CalcDirLightContribution(dirLight, mat_ambient, mat_diffuse, mat_specular, normal, fs_in.fragViewPos);

    color = vec4(finalColor, alpha);
}