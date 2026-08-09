#version 330 core
layout (location = 0) in vec3 aPos;

// UNIFORMS passed from my C++ code
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// OLD: THE FFT Audio Data from BoostMax
// NEW: Mac-Safe FFT Audio Data (UNIFORM BUFFER OBJECT)
layout(std140) uniform FFTData {
    float frequencies[256];
};

// DISCO Ring Settings.
const float PI = 3.14159265359;
const float RING_RADIUS = 15.0;

out vec3 FragPos;
out float AudioIntensity;

void main() {

    // Step1: CALCULATE where this SPECIFIC cube belongs in THE 3D CIRCLE.
    // CONVERT the InstanceID (0 and 255) into a RADIAN ANGLE (0 to 2*PI)
    float angle = (float(gl_InstanceID) / 256.0) * (2.0 * PI);

    float xOffSet = cos(angle) * RING_RADIUS;
    float zOffSet = sin(angle) * RING_RADIUS;

    // Step2: FETCH the audio frequency FOR THIS specific CUBE.
    // BASS frequencies will be at ID 0, TREBLE at ID 255
    float freqHeight = frequencies[gl_InstanceID] * 10.0;   // SCALE the height.
    AudioIntensity = frequencies[gl_InstanceID];            // PASS to fragment shader for glowing colors.

    // Step3: TRANSFORM the cube GEOMETRY.
    vec3 animatedPos = aPos;

    // SCALE the Y-axis of THE CUBE based on the AUDIO FREQUENCY.
    // I ONLY scale the top of the cube (if Y > 0), KEEPING the bottom stuck to the FLOOR.
    if (animatedPos.y > 0.0) {
        animatedPos.y += rawIntensity * 20.0;
    }

    // MOVE the CUBE to its position in the circle.
    animatedPos.x *= 0.15;
    animatedPos.z *= 0.15;

    animatedPos.x += xOffSet;
    animatedPos.z += zOffSet;
    
    // Step4: OUTPUT final 3D position.
    vec4 worldPos = model * vec4(animatedPos, 1.0);
    FragPos = vec3(worldPos);
    gl_Position = projection * view * worldPos;
}