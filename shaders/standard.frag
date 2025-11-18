#version 450 core

out vec4 FragColor;

in VS_OUT
{
    vec3 worldPos;
    vec3 modelPos;  // Model space position for lantern gradient
    vec3 normal;
    vec3 color;
    vec2 uv;
} fs_in;

uniform vec3 uCameraPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uAmbientSky;
uniform vec3 uAmbientGround;
uniform vec3 uFogColor;
uniform float uFogNear;
uniform float uFogFar;
uniform int uUseTexture;
uniform sampler2D uDiffuse;
uniform float uTextureQualityNearDistance;
uniform float uTextureQualityFarDistance;
uniform float uTextureQualityMinFactor;
uniform sampler2D uEnvironmentDay;
uniform sampler2D uEnvironmentNight;
uniform float uEnvironmentBlend;
uniform int uHasEnvironmentMap;
uniform int uMaterialMode;
uniform int uLanternLightCount;
uniform vec3 uLanternLightPos[32];
uniform vec3 uLanternLightColor[32];
uniform float uLanternLightIntensity[32];
uniform float uLanternLightRadius[32];

float hash31(vec3 p)
{
    return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

float proceduralHeight(vec3 worldPos)
{
    vec3 p = worldPos * 0.0012;
    float h = sin(p.x * 11.0 + p.z * 6.0) * 0.5;
    h += cos(p.z * 19.0 + p.y * 4.0) * 0.35;
    h += sin((p.x - p.z) * 5.5) * 0.25;
    return h;
}

vec2 directionToLatLong(vec3 dir)
{
    float phi = atan(dir.z, dir.x);
    float theta = acos(clamp(dir.y, -1.0, 1.0));
    return vec2(phi / (2.0 * 3.14159265) + 0.5, theta / 3.14159265);
}

vec3 sampleEnvironment(vec3 dir)
{
    vec2 uv = directionToLatLong(normalize(dir));
    if (uHasEnvironmentMap == 0)
    {
        float horizon = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
        return mix(vec3(0.25, 0.3, 0.35), vec3(0.85, 0.9, 1.0), horizon);
    }
    vec3 day = texture(uEnvironmentDay, uv).rgb;
    vec3 night = texture(uEnvironmentNight, uv).rgb;
    return mix(day, night, clamp(uEnvironmentBlend, 0.0, 1.0));
}

vec3 brushedMetalNormal(vec3 normal, vec3 worldPos)
{
    vec2 cyl = vec2(worldPos.x, worldPos.z);
    float len = max(length(cyl), 0.001);
    vec2 dir = cyl / len;
    float angle = atan(cyl.y, cyl.x);
    float swirl = sin(angle * 80.0) * 0.05;
    float radial = cos(len * 0.01) * 0.02;
    vec3 tangent = normalize(vec3(-dir.y, 0.0, dir.x));
    vec3 radialDir = normalize(vec3(dir.x, 0.0, dir.y));
    vec3 perturbed = normal + tangent * swirl + radialDir * radial;
    return normalize(perturbed);
}

vec3 groundBumpNormal(vec3 normal, vec3 worldPos)
{
    const float eps = 15.0;
    float hx = proceduralHeight(worldPos + vec3(eps, 0.0, 0.0));
    float hx2 = proceduralHeight(worldPos - vec3(eps, 0.0, 0.0));
    float hy = proceduralHeight(worldPos + vec3(0.0, eps, 0.0));
    float hy2 = proceduralHeight(worldPos - vec3(0.0, eps, 0.0));
    float hz = proceduralHeight(worldPos + vec3(0.0, 0.0, eps));
    float hz2 = proceduralHeight(worldPos - vec3(0.0, 0.0, eps));
    vec3 grad = vec3(hx - hx2, hy - hy2, hz - hz2);
    return normalize(normal + grad * 0.35);
}

vec3 flagMicroNormal(vec3 normal, vec2 uv, vec3 worldPos)
{
    float fiber = sin(uv.y * 900.0) * 0.015;
    float ripple = cos(uv.x * 600.0 + worldPos.y * 0.02) * 0.01;
    vec3 perturb = vec3(fiber, ripple, -fiber * 0.5);
    return normalize(normal + perturb);
}

vec3 sampleGroundTriplanar(vec3 worldPos, vec3 normal)
{
    float scale = 0.0025;
    vec3 blending = abs(normal);
    blending = pow(blending, vec3(4.0));
    blending /= max(blending.x + blending.y + blending.z, 0.0001);
    vec2 coordX = fract(worldPos.zy * scale);
    vec2 coordY = fract(worldPos.xz * scale);
    vec2 coordZ = fract(worldPos.xy * scale);
    vec3 xSample = texture(uDiffuse, coordX).rgb;
    vec3 ySample = texture(uDiffuse, coordY).rgb;
    vec3 zSample = texture(uDiffuse, coordZ).rgb;

    float detail1 = 0.5 + 0.5 * sin(worldPos.x * 0.01 + worldPos.z * 0.02);
    float detail2 = 0.5 + 0.5 * sin(worldPos.x * 0.02 - worldPos.z * 0.015);
    float detail3 = 0.5 + 0.5 * cos(worldPos.x * 0.005 + worldPos.z * 0.005);
    float detailVal = (detail1 + detail2 + detail3) / 3.0;
    vec3 detail = vec3(detailVal);

    xSample = mix(xSample, detail, 0.2);
    ySample = mix(ySample, detail, 0.2);
    zSample = mix(zSample, detail, 0.2);
    return xSample * blending.x + ySample * blending.y + zSample * blending.z;
}

void main()
{
    vec3 normal = normalize(fs_in.normal);
    vec3 sunDir = normalize(uSunDir);
    vec3 viewDir = normalize(uCameraPos - fs_in.worldPos);

    if (uMaterialMode == 1)
    {
        normal = brushedMetalNormal(normal, fs_in.worldPos);
    }
    else if (uMaterialMode == 2)
    {
        normal = groundBumpNormal(normal, fs_in.worldPos);
    }
    else if (uMaterialMode == 3)
    {
        normal = flagMicroNormal(normal, fs_in.uv, fs_in.worldPos);
    }

    // 增强漫反射光照
    float diffuseFactor = max(dot(normal, -sunDir), 0.0);
    // 使用更亮的漫反射，并添加最小亮度保证
    vec3 diffuse = uSunColor * (diffuseFactor * 1.2 + 0.1);

    // 增强镜面反射
    vec3 halfDir = normalize(viewDir - sunDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), 32.0);  // 降低高光锐度，增加范围
    // Reduce specular intensity for cloth materials (material mode 3)
    float specularIntensity = (uMaterialMode == 3) ? 0.15 : 0.4;  // Much lower for cloth
    vec3 specular = uSunColor * spec * specularIntensity;  // 增强镜面反射强度
    if (uMaterialMode == 3)
    {
        vec3 dpdu = dFdx(fs_in.worldPos);
        vec3 tangent = normalize(dpdu);
        float anisSpec = pow(max(dot(normalize(viewDir - sunDir), tangent), 0.0), 70.0);
        // Reduce anisotropic specular for cloth-like appearance (less shiny)
        specular += uSunColor * anisSpec * 0.15;  // Reduced from 0.6 to 0.15 for cloth
    }

    // 增强半球环境光
    float hemi = normal.y * 0.5 + 0.5;
    vec3 ambient = mix(uAmbientGround, uAmbientSky, clamp(hemi, 0.0, 1.0));
    // 增强环境光强度，确保即使在没有直射光的地方也能看到颜色
    ambient *= 1.5;

    vec3 albedo = fs_in.color;
    if (uUseTexture == 1)
    {
        // Enhanced texture sampling with anti-aliasing to reduce moiré patterns
        vec2 uvDx = dFdx(fs_in.uv);
        vec2 uvDy = dFdy(fs_in.uv);
        
        // Calculate distance to camera for distance-based quality adjustments
        float texDistanceToCamera = length(uCameraPos - fs_in.worldPos);
        
        // Calculate smooth distance-based quality factor
        float distanceRange = max(1.0, uTextureQualityFarDistance - uTextureQualityNearDistance);
        float minQuality = clamp(uTextureQualityMinFactor, 0.05, 1.0);
        float normalizedDistance = clamp((texDistanceToCamera - uTextureQualityNearDistance) / distanceRange, 0.0, 1.0);
        float smoothDistance = smoothstep(0.0, 1.0, normalizedDistance);
        
        // Larger filter footprint for distant surfaces reduces moiré
        float maxFilterScale = 1.0 / minQuality;  // e.g. minQuality 0.3 => ~3.33x footprint
        float filterScale = mix(1.0, maxFilterScale, smoothDistance);
        vec2 adjustedDx = uvDx * filterScale;
        vec2 adjustedDy = uvDy * filterScale;
        float derivativeMagnitude = max(length(adjustedDx), length(adjustedDy));
        float lodBias = clamp(log2(filterScale), 0.0, 8.0);
        
        vec3 gradSample = textureGrad(uDiffuse, fs_in.uv, adjustedDx, adjustedDy).rgb;
        vec3 lodSoftSample = textureLod(uDiffuse, fs_in.uv, lodBias + 0.75).rgb;
        float derivativeWeight = clamp(derivativeMagnitude * 220.0, 0.0, 1.0);
        float lodBlend = clamp(smoothDistance * 0.65 + derivativeWeight * 0.35, 0.0, 1.0);
        vec3 texColor = mix(gradSample, lodSoftSample, lodBlend);
        
        // Elliptical Poisson supersampling for distant & glancing surfaces (reduces shimmering while moving)
        float sampleRadius = 0.55 * derivativeMagnitude;
        if (sampleRadius > 0.0002)
        {
            const vec2 poisson[16] = vec2[](
                vec2(-0.326, -0.406), vec2(-0.840, -0.074),
                vec2(-0.696,  0.457), vec2(-0.203,  0.621),
                vec2( 0.962, -0.195), vec2( 0.473, -0.480),
                vec2( 0.519,  0.767), vec2( 0.185, -0.893),
                vec2( 0.089, -0.164), vec2( 0.641, -0.065),
                vec2( 0.102,  0.530), vec2(-0.506,  0.355),
                vec2(-0.235, -0.659), vec2(-0.274,  0.121),
                vec2( 0.699,  0.457), vec2(-0.003, -0.901)
            );
            
            // Determine anisotropic sampling axes
            bool useDxPrimary = dot(uvDx, uvDx) >= dot(uvDy, uvDy);
            vec2 primaryAxis = useDxPrimary ? uvDx : uvDy;
            vec2 secondaryAxis = useDxPrimary ? uvDy : uvDx;
            float primaryLen = max(length(primaryAxis), 1e-6);
            float secondaryLen = max(length(secondaryAxis), 1e-6);
            vec2 primaryDir = primaryAxis / primaryLen;
            vec2 secondaryDir = secondaryAxis / secondaryLen;
            float anisotropy = clamp(primaryLen / secondaryLen, 1.0, 6.0);
            
            // Random rotation per fragment to avoid pattern repetition
            float h = fract(sin(dot(fs_in.worldPos.xz, vec2(12.9898, 78.233))) * 43758.5453);
            float angle = h * 6.2831853;
            float s = sin(angle);
            float c = cos(angle);
            mat2 rot = mat2(c, -s, s, c);
            
            int sampleCount = int(clamp(mix(4.0, 14.0, smoothDistance + derivativeWeight * 0.5), 4.0, 14.0));
            vec3 accum = texColor * 2.0;
            float weightSum = 2.0;
            float minorScale = mix(0.55, 0.95, smoothDistance);
            float majorScale = mix(1.0, min(2.5, 0.8 + anisotropy), smoothDistance);
            
            for (int i = 0; i < sampleCount; ++i)
            {
                vec2 jitter = rot * poisson[i];
                vec2 ellipticalOffset = primaryDir * jitter.x * sampleRadius * majorScale +
                                        secondaryDir * jitter.y * sampleRadius * minorScale;
                vec3 sampleColor = textureLod(uDiffuse, fs_in.uv + ellipticalOffset, lodBias + 0.5).rgb;
                
                float w = mix(0.35, 1.0, smoothDistance) * (1.0 - float(i) / float(sampleCount));
                accum += sampleColor * w;
                weightSum += w;
            }
            
            texColor = accum / weightSum;
        }
        
        if (uMaterialMode == 2)
        {
            vec3 triColor = sampleGroundTriplanar(fs_in.worldPos, normal);
            texColor = mix(texColor, triColor, 0.6);
        }
        
        // Enhance color saturation for more vibrant colors
        float maxColor = max(max(texColor.r, texColor.g), texColor.b);
        float minColor = min(min(texColor.r, texColor.g), texColor.b);
        float oldSaturation = maxColor > 0.001 ? (maxColor - minColor) / maxColor : 0.0;
        
        // Increase saturation by 30% for more vibrant colors
        float newSaturation = min(1.0, oldSaturation * 1.3);
        
        if (newSaturation > 0.001 && maxColor > minColor)
        {
            float delta = maxColor - minColor;
            vec3 colorDelta = (texColor - vec3(minColor)) / delta;
            float saturationBoost = (newSaturation - oldSaturation) * maxColor;
            texColor = vec3(minColor) + colorDelta * (delta + saturationBoost);
        }
        
        // Slight brightness boost
        texColor = pow(texColor, vec3(0.88));  // Slightly brighter
        
        // Mix texture with vertex color
        albedo = mix(albedo, texColor * albedo, 0.9);
    }
    else
    {
        // Enhance vertex color saturation
        float maxColor = max(max(albedo.r, albedo.g), albedo.b);
        float minColor = min(min(albedo.r, albedo.g), albedo.b);
        float saturation = maxColor > 0.001 ? (maxColor - minColor) / maxColor : 0.0;
        saturation = min(1.0, saturation * 1.25);  // Increase saturation
        
        if (saturation > 0.001 && maxColor > minColor)
        {
            float delta = maxColor - minColor;
            vec3 colorDelta = (albedo - vec3(minColor)) / delta;
            float oldSaturation = delta / maxColor;
            float saturationBoost = (saturation - oldSaturation) * maxColor;
            albedo = vec3(minColor) + colorDelta * (delta + saturationBoost);
        }
        
        albedo = pow(albedo, vec3(0.85));  // Slight brightness boost
    }

    // Enhanced lighting calculation with better color vibrancy
    vec3 lighting = ambient + diffuse + specular;
    lighting = max(lighting, vec3(0.35));  // Slightly higher minimum lighting
    
    // Alpha value for lanterns (will be set in material mode 4)
    float lanternAlpha = 1.0;
    
    // For lanterns (mode 4), skip normal lighting - they are self-illuminated
    vec3 baseColor;
    if (uMaterialMode == 4)
    {
        // For lanterns, start with black - will be set in material mode 4 section
        // Don't set any color here, let the gradient calculation handle it
        baseColor = vec3(0.0);
    }
    else
    {
        // Multiply albedo with lighting for normal materials
        baseColor = albedo * lighting;
    }

    if (uMaterialMode == 1)
    {
        vec3 envColor = sampleEnvironment(reflect(-viewDir, normal));
        float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 5.0);
        float upFactor = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);
        float envWeight = mix(0.55, 0.92, upFactor);
        baseColor = mix(baseColor, envColor, envWeight);
        baseColor += envColor * (0.1 + fresnel * 0.2);
    }
    else if (uMaterialMode == 2)
    {
        float height = proceduralHeight(fs_in.worldPos);
        baseColor *= mix(0.85, 1.15, height * 0.5 + 0.5);
    }
    else if (uMaterialMode == 3 && uHasEnvironmentMap == 1)
    {
        vec3 envColor = sampleEnvironment(reflect(-viewDir, normal));
        // Reduce environment reflection for cloth (less mirror-like)
        baseColor = mix(baseColor, envColor, 0.03);  // Reduced from 0.12 to 0.03 for cloth
    }
    
    // Add lantern point lights contribution (illuminating other objects)
    if (uMaterialMode != 4)
    {
        int safeLightCount = uLanternLightCount;
        if (safeLightCount > 32) safeLightCount = 32;
        if (safeLightCount < 0) safeLightCount = 0;
        for (int i = 0; i < safeLightCount; ++i)
        {
            vec3 lightVec = uLanternLightPos[i] - fs_in.worldPos;
            float dist = length(lightVec);
            vec3 lightDir = dist > 0.0 ? lightVec / dist : vec3(0.0, 1.0, 0.0);
            float influence = max(0.0, 1.0 - dist / max(uLanternLightRadius[i], 1.0));
            // Increased attenuation for brighter lighting (reduced distance falloff)
            float attenuation = uLanternLightIntensity[i] * influence / (1.0 + dist * dist * 0.00005);
            float diff = max(dot(normal, lightDir), 0.0);
            // Add both diffuse and ambient contribution from lantern light
            baseColor += uLanternLightColor[i] * attenuation * (diff + 0.3);
        }
        
        // Add emissive glow for objects near lanterns (but not lanterns themselves)
        vec3 emissiveGlow = vec3(0.0);
        for (int i = 0; i < safeLightCount; ++i)
        {
            vec3 toLight = uLanternLightPos[i] - fs_in.worldPos;
            float distToLight = length(toLight);
            // If very close to a lantern (within 200 units), add strong emissive glow
            if (distToLight < 200.0)
            {
                float glowStrength = 1.0 - smoothstep(0.0, 200.0, distToLight);
                // Much stronger glow - multiply by intensity and add base color
                float glowIntensity = uLanternLightIntensity[i] * 2.0;
                emissiveGlow += uLanternLightColor[i] * glowIntensity * glowStrength * 1.5;
            }
        }
        baseColor += emissiveGlow;
    }
    
    // Lantern material: paper material with internal light source showing gradient (opaque)
    if (uMaterialMode == 4)
    {
        // Orange-red light color
        vec3 orangeRedColor = vec3(1.0, 0.5, 0.2); // Bright orange-red
        
        // Each lantern is self-illuminated - calculate distance from fragment to center
        // Use model space position (passed from vertex shader) to calculate distance to center
        // Model space origin (0,0,0) is assumed to be the center of the lantern
        float distToCenter = length(fs_in.modelPos);
        
        // Fire-like gradient: bright warm center fading to dimmer edges
        // Center: yellow-orange, middle: red-orange, edge: gray (not black)
        
        // Calculate distance-based gradient with smoother falloff
        float maxLightDist = 300.0; // Radius to cover entire lantern
        float normalizedDist = clamp(distToCenter / maxLightDist, 0.0, 1.0);
        
        // Create smoother gradient layers
        // Core: very bright at center
        float fireCore = pow(1.0 - normalizedDist, 3.5); // Strong falloff but not too extreme
        // Inner glow: extends further
        float fireInnerGlow = pow(1.0 - normalizedDist, 1.8);
        // Outer glow: extends even further for smooth transition
        float fireOuterGlow = pow(1.0 - normalizedDist, 1.0);
        
        // Color layers: orange -> red-orange -> dark gray-black
        vec3 fireCoreColor = vec3(1.0, 0.6, 0.2); // Bright orange (center)
        vec3 fireInnerColor = vec3(1.0, 0.5, 0.2); // Orange to red-orange transition
        vec3 fireOuterColor = vec3(1.0, 0.4, 0.15); // Red-orange
        vec3 edgeColor = vec3(0.25, 0.2, 0.18); // Dark gray-black (slight gray tint, not pure black)
        
        // Build fire color with layered approach
        vec3 fireColor = vec3(0.0);
        fireColor += fireCoreColor * fireCore * 4.0; // Even brighter core (increased from 3.2)
        fireColor += fireInnerColor * fireInnerGlow * 1.4; // Inner glow (increased from 1.3)
        fireColor += fireOuterColor * fireOuterGlow * 0.7; // Outer glow
        
        // Mix between fire color and gray edge color based on distance
        // Use smoother curve to avoid too much black
        float gradientFactor = pow(1.0 - normalizedDist, 0.9); // Softer falloff
        baseColor = mix(edgeColor, fireColor, gradientFactor);
        
        // Add extra brightness boost at very center (within 25% of radius)
        if (normalizedDist < 0.25)
        {
            float centerBoost = pow(1.0 - normalizedDist / 0.25, 4.0);
            baseColor += fireCoreColor * centerBoost * 1.6; // Even stronger center boost (increased from 1.2)
        }
        
        // Make the bottom 10% of the lantern dark gray-black
        // Use model space Y coordinate to determine bottom portion
        // Assuming model center is at origin, bottom 10% is the lowest Y values
        
        float modelY = fs_in.modelPos.y;
        
        // Estimate model height range (adjust based on your lantern model size)
        // For a typical lantern, if model spans from -height/2 to +height/2
        // Bottom 10% would be from -height/2 to -height/2 + height*0.1
        float estimatedModelHeight = 300.0; // Adjust this to match your lantern model height
        float modelBottom = -estimatedModelHeight * 0.5; // Bottom of model
        float bottom10PercentTop = modelBottom + estimatedModelHeight * 0.1; // Top of bottom 10%
        
        // Check if this fragment is in the bottom 10% of the model
        if (modelY >= modelBottom && modelY <= bottom10PercentTop)
        {
            // Calculate progress within the bottom 10% (0.0 = very bottom, 1.0 = top of bottom 10%)
            float bottomRange = estimatedModelHeight * 0.1;
            float bottomProgress = (modelY - modelBottom) / bottomRange;
            
            // Blend to dark gray-black (not pure black)
            vec3 bottomColor = vec3(0.15, 0.12, 0.1); // Dark gray-black
            // Use smoothstep for smooth transition at the boundary
            float blendFactor = 1.0 - smoothstep(0.0, 0.2, bottomProgress); // Stronger at bottom, fades at top
            baseColor = mix(baseColor, bottomColor, blendFactor);
        }
        
        // Add transition glow in mid-range to smooth out the gradient
        // This helps transition from red-orange to gray
        if (normalizedDist > 0.5 && normalizedDist < 1.0)
        {
            float transitionGlow = smoothstep(1.0, 0.5, normalizedDist);
            // Mix red-orange with gray for smooth transition
            vec3 transitionColor = mix(edgeColor, fireOuterColor, transitionGlow * 0.6);
            baseColor = mix(baseColor, transitionColor, transitionGlow * 0.3);
        }
        
        // External halo: more visible and extensive glow around the lantern
        // Calculate halo based on distance from center
        float haloIntensity = 0.0;
        
        // Inner halo: close to lantern surface (stronger)
        float innerHaloStart = maxLightDist * 0.8;
        float innerHaloEnd = maxLightDist * 1.3;
        if (distToCenter > innerHaloStart && distToCenter < innerHaloEnd)
        {
            float haloDist = (distToCenter - innerHaloStart) / (innerHaloEnd - innerHaloStart);
            haloIntensity += (1.0 - smoothstep(0.0, 1.0, haloDist)) * 0.4; // Stronger inner halo
        }
        
        // Mid halo: medium range
        float midHaloStart = maxLightDist * 1.3;
        float midHaloEnd = maxLightDist * 2.0;
        if (distToCenter > midHaloStart && distToCenter < midHaloEnd)
        {
            float haloDist = (distToCenter - midHaloStart) / (midHaloEnd - midHaloStart);
            haloIntensity += (1.0 - smoothstep(0.0, 1.0, haloDist)) * 0.25;
        }
        
        // Outer halo: extending further out
        float outerHaloStart = maxLightDist * 2.0;
        float outerHaloEnd = maxLightDist * 3.5;
        if (distToCenter > outerHaloStart && distToCenter < outerHaloEnd)
        {
            float haloDist = (distToCenter - outerHaloStart) / (outerHaloEnd - outerHaloStart);
            haloIntensity += (1.0 - smoothstep(0.0, 1.0, haloDist)) * 0.15;
        }
        
        // Add external halo glow (more visible orange-red glow around lantern)
        baseColor += orangeRedColor * haloIntensity * 0.8; // Stronger halo contribution
        
        // No external lighting applied - fully self-illuminated
        
        // Clamp to preserve orange-red color but allow brighter fire core
        // Keep red dominant, green and blue lower to maintain orange-red hue
        baseColor.r = max(0.0, min(baseColor.r, 1.0));
        baseColor.g = max(0.0, min(baseColor.g, 0.6));
        baseColor.b = max(0.0, min(baseColor.b, 0.2));
        
        lanternAlpha = 1.0;
    }
    
    // Enhanced color saturation boost (skip for lanterns - they are self-illuminated)
    if (uMaterialMode != 4)
    {
        float maxColor = max(max(baseColor.r, baseColor.g), baseColor.b);
        float minColor = min(min(baseColor.r, baseColor.g), baseColor.b);
        float saturation = maxColor > 0.001 ? (maxColor - minColor) / maxColor : 0.0;
        
        // Boost saturation by 20% for more vibrant colors
        saturation = min(1.0, saturation * 1.2);
        if (saturation > 0.001 && maxColor > minColor)
        {
            float delta = maxColor - minColor;
            vec3 colorDelta = (baseColor - vec3(minColor)) / delta;
            float oldSaturation = delta / maxColor;
            float saturationBoost = (saturation - oldSaturation) * maxColor;
            baseColor = vec3(minColor) + colorDelta * (delta + saturationBoost);
        }
        
        // Enhanced tone mapping for better contrast and vibrancy
        baseColor = baseColor / (baseColor + vec3(0.8));  // Slightly modified Reinhard for more contrast
        baseColor = pow(baseColor, vec3(0.92));  // Slight brightness boost
    }

    float distanceToCamera = length(uCameraPos - fs_in.worldPos);
    float fogFactor = clamp((uFogFar - distanceToCamera) / (uFogFar - uFogNear), 0.0, 1.0);
    
    // For lanterns, completely skip fog effect
    vec3 finalColor;
    if (uMaterialMode == 4)
    {
        // Lanterns are self-illuminated, completely ignore fog
        // Use baseColor directly - it already has the fire gradient effect
        finalColor = baseColor;
    }
    else
    {
        finalColor = mix(uFogColor, baseColor, fogFactor);
    }

    // Alpha channel: opaque for most materials, semi-transparent for lanterns
    float alpha = (uMaterialMode == 4) ? lanternAlpha : 1.0;

    FragColor = vec4(finalColor, alpha);
}

