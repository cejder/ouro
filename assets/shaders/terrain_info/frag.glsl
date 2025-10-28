#version 330
in float normalizedHeight;
out vec4 finalColor;

void main() {
    float h = normalizedHeight;

    // Contour lines every 5% height
    float contour = fract(h * 20.0);
    bool isLine = contour < 0.1;

    // Simple blue->green->red gradient
    vec3 color = vec3(1.0 - h, h * (1.0 - h) * 4.0, h);

    // Darken on contour lines
    if (isLine) color *= 0.3;

    finalColor = vec4(color, 0.7);
}
