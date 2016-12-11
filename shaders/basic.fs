#version 330
uniform sampler2D myTexture;
uniform vec4 couleur;
uniform int posx;
uniform int posy;

in  vec2 vsoTexCoord;
out vec4 fragColor;

void main(void) {
  fragColor = texture(myTexture, vsoTexCoord.st);// * couleur;
}
