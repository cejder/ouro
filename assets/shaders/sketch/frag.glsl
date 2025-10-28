#version 410

in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;
uniform vec2 resolution;
uniform vec4 majorColor;
uniform vec4 minorColor;
uniform float time;

// Feature toggles
#define ENABLE_EDGE_DETECTION 1
#define ENABLE_ORIGINAL_BLEND 1
#define ENABLE_BLOOM 1

// Core constants
#define LUMA_COEFFS vec3(0.299, 0.587, 0.114)
#define ORIGINAL_BLEND_AMOUNT 0.4

// Edge detection parameters
#define EDGE_SAMPLE_SCALE 0.65
#define EDGE_REDUCE_MULTIPLIER 0.25
#define EDGE_REDUCE_MINIMUM 0.0312
#define EDGE_SPAN_MAXIMUM 8.0
#define EDGE_STRENGTH_BASE 1.0
#define EDGE_THRESHOLD 0.006
#define MONO_THRESHOLD 0.04

// Distance-based transition
#define RADIAL_TRANSITION_START 0.33
#define RADIAL_TRANSITION_END 0.85
#define NOISE_INTENSITY 0.15
#define DITHER_INFLUENCE 0.5

// Bloom parameters
#define BLOOM_INTENSITY 0.5
#define BLOOM_SAMPLE_COUNT 8
#define BLOOM_SAMPLE_RADIUS 0.004
#define BLOOM_COLOR_TINT vec3(0.75, 0.85, 1.05)
#define BLOOM_WEIGHT_SCALE 0.06

// Utility functions
float calculateLuma(vec3 color) {
    return dot(color, LUMA_COEFFS);
}

float pseudoRandom(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

float bayerDitherPattern(vec2 screenPos) {
    const mat4 bayerMatrix = mat4(vec4(0.0, 8.0, 2.0, 10.0), vec4(12.0, 4.0, 14.0, 6.0), vec4(3.0, 11.0, 1.0, 9.0), vec4(15.0, 7.0, 13.0, 5.0)) / 16.0;

    ivec2 pos = ivec2(screenPos) & 3;
    return bayerMatrix[pos.x][pos.y];
}

void main() {
    vec2 texelSize = 1.0 / (resolution * 1.0);
    vec4 centerSample = texture(texture0, fragTexCoord);

    // Early exit for transparent pixels
    if (centerSample.a < 0.01) {
        finalColor = centerSample;
        return;
    }

    vec3 centerColor = centerSample.rgb;
    float centerLuma = calculateLuma(centerColor);

    // Initialize color with base mixing
    vec3 resultColor = mix(majorColor.rgb * vec3(0.9, 0.95, 1.1), minorColor.rgb * vec3(0.95, 0.9, 1.05), centerLuma);

#if ENABLE_EDGE_DETECTION
    // Sample neighborhood for edge detection
    vec3 colorNW = texture(texture0, fragTexCoord + vec2(-1.0, -1.0) * texelSize).rgb;
    vec3 colorNE = texture(texture0, fragTexCoord + vec2(1.0, -1.0) * texelSize).rgb;
    vec3 colorSW = texture(texture0, fragTexCoord + vec2(-1.0, 1.0) * texelSize).rgb;
    vec3 colorSE = texture(texture0, fragTexCoord + vec2(1.0, 1.0) * texelSize).rgb;

    float lumaNW = calculateLuma(colorNW);
    float lumaNE = calculateLuma(colorNE);
    float lumaSW = calculateLuma(colorSW);
    float lumaSE = calculateLuma(colorSE);

    float lumaMin = min(centerLuma, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(centerLuma, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;

    // Distance-based effects
    float distFromCenter = length(fragTexCoord - 0.5);
    float ditherValue = bayerDitherPattern(gl_FragCoord.xy);
    float noiseValue = pseudoRandom(fragTexCoord) * NOISE_INTENSITY;

    // Apply dithering based on distance from center
    float ditherAmount = mix(0.0, DITHER_INFLUENCE, smoothstep(0.2, 0.6, distFromCenter));
    distFromCenter += (ditherValue + noiseValue - 0.5) * ditherAmount;

    float radialFactor = smoothstep(RADIAL_TRANSITION_START, RADIAL_TRANSITION_END, distFromCenter);
    radialFactor *= (1.0 - 0.2 * (noiseValue + ditherValue));

    // Adaptive thresholds based on distance
    float adaptiveThreshold = mix(EDGE_THRESHOLD, EDGE_THRESHOLD * 3.0, radialFactor);
    float adaptiveMonoThreshold = mix(MONO_THRESHOLD, MONO_THRESHOLD * 3.0, radialFactor);

    // Calculate edge direction
    vec2 edgeDirection;
    edgeDirection.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    edgeDirection.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float reduceStrength = 1.0 + (radialFactor * 1.5);
    float directionReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (EDGE_REDUCE_MULTIPLIER * reduceStrength), EDGE_REDUCE_MINIMUM * reduceStrength);

    float rcpDirMin = 1.0 / (min(abs(edgeDirection.x), abs(edgeDirection.y)) + directionReduce);
    float spanLimit = mix(EDGE_SPAN_MAXIMUM, EDGE_SPAN_MAXIMUM * 0.7, radialFactor);
    edgeDirection = clamp(edgeDirection * rcpDirMin, -spanLimit, spanLimit) * texelSize;

    float edgeStrength = EDGE_STRENGTH_BASE * (1.0 - radialFactor * 0.5);
    edgeStrength *= (1.0 + lumaRange * (1.0 - radialFactor * 0.3));
    edgeDirection *= edgeStrength;

    // Sample along edge direction
    vec3 edgeSampleA = EDGE_SAMPLE_SCALE * (texture(texture0, fragTexCoord + edgeDirection * (1.0 / 3.0 - 0.5)).rgb +
                                            texture(texture0, fragTexCoord + edgeDirection * (2.0 / 3.0 - 0.5)).rgb);

    vec3 edgeSampleB =
        edgeSampleA * 0.5 + 0.25 * (texture(texture0, fragTexCoord + edgeDirection * -0.5).rgb + texture(texture0, fragTexCoord + edgeDirection * 0.5).rgb);

    // Blend edge samples with original based on gradient magnitude
    float gradientScale = mix(0.75, 1.25, radialFactor);
    float gradientMagnitude = length(edgeDirection) * gradientScale;
    edgeSampleB = mix(edgeSampleB, centerColor, smoothstep(0.0, 1.0, gradientMagnitude));

    float edgeLuma = calculateLuma(edgeSampleB);
    float transitionScale = 1.0 + radialFactor * 1.5;
    float edgeTransition = smoothstep(adaptiveThreshold * 0.5, adaptiveThreshold * transitionScale, abs(edgeLuma - lumaMin));
    edgeTransition *= smoothstep(adaptiveThreshold * 0.5, adaptiveThreshold * transitionScale, abs(lumaMax - edgeLuma));

    // Determine final edge color
    vec3 edgeColor = (edgeLuma < lumaMin + adaptiveThreshold || edgeLuma > lumaMax - adaptiveThreshold) ? majorColor.rgb : minorColor.rgb;

    resultColor = (edgeLuma < adaptiveMonoThreshold) ? minorColor.rgb : mix(minorColor.rgb, edgeColor, edgeTransition);
#endif

#if ENABLE_ORIGINAL_BLEND
    // Preserve luminance while blending original color
    float processedLuma = calculateLuma(resultColor);
    float originalLuma = max(centerLuma, 0.001);
    vec3 luminanceMatchedOriginal = centerColor * (processedLuma / originalLuma);
    resultColor = mix(resultColor, luminanceMatchedOriginal, ORIGINAL_BLEND_AMOUNT);
#endif

#if ENABLE_BLOOM
    vec3 bloomAccumulator = vec3(0.0);
    float bloomFalloff = 1.0 - length(fragTexCoord - 0.5) * 0.8;

    // Sample bloom in cross pattern (horizontal + vertical)
    for (int i = 0; i < BLOOM_SAMPLE_COUNT; i++) {
        float sampleOffset = BLOOM_SAMPLE_RADIUS * float(i) / float(BLOOM_SAMPLE_COUNT - 1);
        float sampleWeight = 1.0 - float(i) / float(BLOOM_SAMPLE_COUNT);

        bloomAccumulator += (texture(texture0, fragTexCoord + vec2(sampleOffset, 0.0)).rgb + texture(texture0, fragTexCoord - vec2(sampleOffset, 0.0)).rgb +
                             texture(texture0, fragTexCoord + vec2(0.0, sampleOffset)).rgb + texture(texture0, fragTexCoord - vec2(0.0, sampleOffset)).rgb) *
                            sampleWeight * BLOOM_COLOR_TINT;
    }

    // Add diagonal samples for more complete bloom
    for (int i = 1; i < 3; i++) {
        float diagonalOffset = BLOOM_SAMPLE_RADIUS * 0.7 * float(i);
        float diagonalWeight = 0.5 * (1.0 - float(i) / 3.0);

        bloomAccumulator += (texture(texture0, fragTexCoord + vec2(diagonalOffset, diagonalOffset)).rgb +
                             texture(texture0, fragTexCoord - vec2(diagonalOffset, diagonalOffset)).rgb +
                             texture(texture0, fragTexCoord + vec2(-diagonalOffset, diagonalOffset)).rgb +
                             texture(texture0, fragTexCoord - vec2(-diagonalOffset, diagonalOffset)).rgb) *
                            diagonalWeight * BLOOM_COLOR_TINT;
    }

    bloomAccumulator *= BLOOM_WEIGHT_SCALE;
    resultColor += bloomAccumulator * BLOOM_INTENSITY * bloomFalloff;

    // Add highlight boost for bright areas
    float brightness = calculateLuma(centerColor);
    resultColor += BLOOM_COLOR_TINT * BLOOM_INTENSITY * 0.5 * pow(brightness, 3.0) * bloomFalloff;
#endif

    finalColor = vec4(resultColor, centerSample.a);
}
