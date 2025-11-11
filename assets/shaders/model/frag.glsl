#version 330 core

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

// Output fragment color
out vec4 finalColor;

#define LIGHTS_MAX 10
#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_SPOT 1

struct Fog {
    float density;
    vec4 color;
};

struct Light {
    int enabled;
    int type;
    vec3 position;
    vec3 direction;
    vec4 color;
    float intensity;
    float inner_cutoff;
    float outer_cutoff;
};

// Input uniform variables
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform Light lights[LIGHTS_MAX];
uniform vec4 ambient;
uniform vec3 viewPos;
uniform Fog fog;
uniform int isSelected;

vec3 calculateLighting(vec3 normal, vec3 viewD, vec3 fragPosition) {
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);

    for (int i = 0; i < LIGHTS_MAX; i++) {
        if (lights[i].enabled == 1) {
            vec3 lightDir;
            float attenuation = 1.0;

            if (lights[i].type == LIGHT_TYPE_POINT) {
                // Point light: omnidirectional from position
                lightDir = normalize(lights[i].position - fragPosition);
                float distance = length(lights[i].position - fragPosition);
                attenuation = 1.0 - clamp(distance / lights[i].intensity, 0.0, 1.0);
                attenuation = attenuation * attenuation;
            } else if (lights[i].type == LIGHT_TYPE_SPOT) {
                // Spot light: cone from position in direction
                lightDir = normalize(lights[i].position - fragPosition);
                float distance = length(lights[i].position - fragPosition);

                // Distance attenuation
                float distAttenuation = 1.0 - clamp(distance / lights[i].intensity, 0.0, 1.0);
                distAttenuation = distAttenuation * distAttenuation;

                // Cone attenuation - fixed calculation
                vec3 lightToFrag = normalize(fragPosition - lights[i].position);
                float theta = dot(lightToFrag, normalize(lights[i].direction));
                float epsilon = lights[i].inner_cutoff - lights[i].outer_cutoff;
                float coneAttenuation = clamp((theta - lights[i].outer_cutoff) / epsilon, 0.0, 1.0);

                attenuation = distAttenuation * coneAttenuation;
            }

            float NdotL = max(dot(normal, lightDir), 0.0);
            vec3 diffuseContrib = lights[i].color.rgb * NdotL * attenuation;

            vec3 specularContrib = vec3(0.0);
            if (NdotL > 0.0) {
                vec3 halfwayDir = normalize(lightDir + viewD);
                float specPower = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
                specularContrib = specPower * lights[i].color.rgb * attenuation * 0.3;
            }

            diffuse += diffuseContrib;
            specular += specularContrib;
        }
    }

    return diffuse + specular;
}

vec4 applyFog(vec4 color, vec3 viewPos, vec3 fragPosition, Fog fog) {
    float dist = length(viewPos - fragPosition);
    float fogFactor = exp(-(dist * fog.density) * (dist * fog.density));
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    // Calculate luminance to detect bright areas (light sources)
    float luminance = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    // Reduce fog effect for bright areas to allow light penetration
    float lightPenetration = smoothstep(0.1, 0.8, luminance);
    float adjustedFogFactor = mix(fogFactor, 1.0, lightPenetration * 0.7);
    return mix(fog.color, color, adjustedFogFactor);
}

vec4 applyDithering(vec4 color) {
    float ditherMatrix[4] = float[4](0.0, 0.5, 0.75, 0.25);
    float ditherValue = ditherMatrix[int(mod(gl_FragCoord.x, 2.0)) + int(mod(gl_FragCoord.y, 2.0)) * 2];
    color.rgb += (ditherValue - 0.5) / 255.0;
    return color;
}

void main() {
    // Texel color fetching from texture sampler
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 normal = normalize(fragNormal);
    vec3 viewD = normalize(viewPos - fragPosition);
    // Calculate lighting using improved model
    vec3 lightResult = calculateLighting(normal, viewD, fragPosition);
    // Combine texture color, diffuse color, and lighting
    finalColor = texelColor * vec4(lightResult, 1.0) * colDiffuse;
    // Add ambient light
    finalColor += texelColor * ambient * 0.2;
    // Apply fog
    finalColor = applyFog(finalColor, viewPos, fragPosition, fog);
    // Apply dithering
    finalColor = applyDithering(finalColor);

    // Apply selection highlight
    if (isSelected == 1) {
        vec3 selectionColor = vec3(0.3, 1.0, 0.4); // Bright RTS green
        float rimPower = 1.0 - max(dot(normal, viewD), 0.0);
        rimPower = pow(rimPower, 1.5); // Wider rim (lower power = wider)
        finalColor.rgb += selectionColor * rimPower * 1.2; // Much brighter
    }
}
