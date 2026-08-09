#version 330 core

// OUTPUT pixel color to the SCREEN.
out vec4 FragColor;

// DATA PASSED in from spectrum.vert
in vec3 FragPos;
in float AudioIntensity;

// MASTER alpha CONTROL sent from the C++ SpectrumRenderer (for the MIRROR REFLECTION)
uniform float alphaMultiplier;

void main() {
    // ==========================================
    // 1. Neon Color Palette.
    // ==========================================
    // QUIET frequencies rest a Cyberpunk Cyan.
    // WHEN the FFT hits HARD, it transition to an Aggressive Magenta/Pink.
    vec3 baseColor = vec3(0.0, 0.8, 1.0);
    vec3 peakColor = vec3(1.0, 0.0, 0.8);

    // ==========================================
    // 2. Intensity Mapping.
    // ==========================================
    // CLAMP the audio intensity to PREVENT extreme 2.5x BoostMax Spikes.
    // FROM causing visual artifacting in the color math.
    float safeIntensity = clamp(AudioIntensity, 0.0, 1.0);

    // MIX the two colors based on exactly how LOUD this specific FREQUENCY bin is.
    vec3 currentAlbedo = mix(baseColor, peakColor, safeIntensity);

    // ==========================================
    // 3. Emissive Bloom Overdrive.
    // ==========================================
    // AS the CUBE gets TALLER (LOUDER), I inject pure white light into the CORE.
    // OF the color, MAKING it look like an ACTIVE LED or NEON TUBE.
    vec3 emissiveGlow = currentAlbedo + (vec3(0.4) * safeIntensity);
    
    // ==========================================
    // 4. Final Render & Mirror Physics.
    // ==========================================
    // SET the final color, THE ALPHA channel is multiplied by my uniform.
    // (1.0 = full brightness for THE REAL CUBES, 0.25 = dark and ghostly for the floor reflection)
    FragColor = vec4(emissiveGlow, 1.0 * alphaMultiplier);
}