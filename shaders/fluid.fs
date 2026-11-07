#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D u_Texture;

void main() {
    vec3 color = texture(u_Texture, TexCoord).rgb;
    // Slight gamma adjustment and contrast boost for rich, fluid colors
    color = pow(color, vec3(0.92));
    FragColor = vec4(color, 1.0);
}

