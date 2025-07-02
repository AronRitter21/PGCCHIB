#version 410

in vec2 texture_coords;

uniform sampler2D sprite;
uniform float offsetx;
uniform float offsety;

out vec4 frag_color;

void main() {
    vec2 coords = vec2(texture_coords.x + offsetx, texture_coords.y + offsety);
    vec4 texel = texture(sprite, coords);
    if (texel.a < 0.1) discard;
    frag_color = texel;
}