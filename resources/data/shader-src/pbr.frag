#version 450 core
#extension GL_ARB_bindless_texture : require
out vec4 frag_col;

layout (location = 0) in vec2 uv;
layout (location = 1) in vec3 world_pos;
layout (location = 2) in vec3 normal;

layout (std140, binding = 0) uniform SceneData
{
	mat4 projection;
	mat4 view;
	vec3 camera_pos;
};

layout (std140, binding = 1) uniform MeshData
{
	mat4 model;
};

struct Material
{
	vec2 atlasBegin;
	vec2 atlasEnd;
	float metallic;
	float roughness;
	float ao;
};

layout (std430, binding = 2) buffer MaterialBuffer
{
	Material materials[20];
	int primitiveToMaterial[];
};

uniform samplerCube environmentMap;
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfMap;
uniform sampler2D albedoAtlas;

const float PI = 3.14159265359;
  
float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlick(float cosTheta, vec3 F0);
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);

void main()
{		
    int       matIdx    = primitiveToMaterial[gl_PrimitiveID];
    Material  mat       = materials[matIdx];
    float     roughness = mat.roughness;
    float     metallic  = mat.metallic;
    float     ao        = mat.ao;
    vec2      real_uv   = vec2(mat.atlasBegin) + (vec2(mat.atlasEnd) - vec2(mat.atlasBegin)) * uv;

	vec3 albedo = pow(texture(albedoAtlas, real_uv).rgb, vec3(2.2));
    vec3 N = normalize(normal);
    vec3 V = normalize(camera_pos - world_pos);
    vec3 R = reflect(-V, N);

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);
	           
    // reflectance equation
    vec3 Lo = vec3(0.0);
//    for(int i = 0; i < lights_count; ++i) 
//    {
//        // calculate per-light radiance
//        vec3 L = normalize(light_pos[i] - world_pos);
//        vec3 H = normalize(V + L);
//        float distance    = length(light_pos[i] - world_pos);
//        float attenuation = 1.0 / (distance * distance);
//        vec3 radiance     = light_col[i] * attenuation;        
//        
//        // cook-torrance brdf
//        float NDF = DistributionGGX(N, H, roughness);        
//        float G   = GeometrySmith(N, V, L, roughness);      
//        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       
//        
//        vec3  numerator   = NDF * G * F;
//        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
//        vec3  specular    = numerator / denominator;  
//        
//        vec3 kS = F;
//        vec3 kD = vec3(1.0) - kS;
//        kD *= 1.0 - metallic;	  
//            
//        // add to outgoing radiance Lo
//        float NdotL = max(dot(N, L), 0.0);                
//        Lo += (kD * albedo / PI + specular) * radiance * NdotL; 
//    }   
//
    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD     *= 1.0 - metallic;

    vec3 irradiance = (1 == 1) ? texture(irradianceMap, N).rgb : vec3(0.03);
    vec3 diffuse    = irradiance * albedo;

    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefiltered_col = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 env_brdf = texture(brdfMap, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specular = prefiltered_col * (F * env_brdf.x + env_brdf.y);

    vec3 ambient    = (kD * diffuse + specular) * ao;

    vec3 color   = ambient + Lo;
	
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));  
   
    frag_col = vec4(color, 1.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}  

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}   