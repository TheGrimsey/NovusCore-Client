#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform SharedUniformBufferObject 
{
    vec4 color;
} panelUbo;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() 
{
	outColor = texture(texSampler, fragTexCoord);
	outColor *= panelUbo.color;
}