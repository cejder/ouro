#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;

// Input uniform values
uniform samplerCube environmentMap;
uniform bool vflipped;
uniform bool doGamma;
uniform vec4 ambient;

// Output fragment color
out vec4 finalColor;

void main() {
    // Fetch color from texture map
    vec3 color = vec3(0.0);

    if (vflipped)
        color = texture(environmentMap, vec3(fragPosition.x, -fragPosition.y, fragPosition.z)).rgb;
    else
        color = texture(environmentMap, fragPosition).rgb;

    if (doGamma)  // Apply gamma correction
    {
        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0 / 2.2));
    }

    // Calculate final fragment color including ambient color
    vec3 finalColorRGB = color + ambient.rgb * ambient.a;  // assuming ambient.a is used to scale the ambient.rgb

    finalColor = vec4(finalColorRGB, 1.0);
}
