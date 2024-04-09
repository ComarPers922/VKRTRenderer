#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../shared_with_shaders.h"

layout(set = SWS_SCENE_AS_SET,     binding = SWS_SCENE_AS_BINDING)            uniform accelerationStructureEXT Scene;
layout(set = SWS_RESULT_IMAGE_SET, binding = SWS_RESULT_IMAGE_BINDING, rgba8) uniform image2D ResultImage;

layout(set = SWS_CAMDATA_SET,      binding = SWS_CAMDATA_BINDING, std140)     uniform AppData {
    UniformParams Params;
};
layout(set = SWS_OBJ_ATTR_SET, binding = 0) readonly buffer ObjectAttribute
{
    ObjAttri objAttri;
} ObjAttris[];

layout(location = SWS_LOC_PRIMARY_RAY) rayPayloadEXT RayPayload PrimaryRay;
layout(location = SWS_LOC_SHADOW_RAY)  rayPayloadEXT ShadowRayPayload ShadowRay;

const float kBunnyRefractionIndex = 1.0f / 1.31f; // ice

vec3 CalcRayDir(vec2 screenUV, float aspect) {
    vec3 u = Params.camSide.xyz;
    vec3 v = Params.camUp.xyz;

    const float planeWidth = tan(Params.camNearFarFov.z * 0.5f);

    u *= (planeWidth * aspect);
    v *= planeWidth;

    const vec3 rayDir = normalize(Params.camDir.xyz + (u * screenUV.x) - (v * screenUV.y));
    return rayDir;
}

void main() {
    const vec2 curPixel = vec2(gl_LaunchIDEXT.xy);
    const vec2 bottomRight = vec2(gl_LaunchSizeEXT.xy - 1);

    const vec2 uv = (curPixel / bottomRight) * 2.0f - 1.0f;

    const float aspect = float(gl_LaunchSizeEXT.x) / float(gl_LaunchSizeEXT.y);

    vec3 origin = Params.camPos.xyz;
    vec3 direction = CalcRayDir(uv, aspect);

    const uint rayFlags = gl_RayFlagsOpaqueEXT;
    const uint shadowRayFlags = gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT;

    uint cullMask = 0xFF;

    const uint stbRecordStride = 1;

    const float tmin = 0.0f;
    const float tmax = Params.camNearFarFov.y;

    vec3 finalColor = vec3(0.0f);

    int recursion = SWS_MAX_RECURSION;
    for (int i = 0; i < recursion; ++i) 
    {
        traceRayEXT(Scene,
                    rayFlags,
                    cullMask,
                    SWS_PRIMARY_HIT_SHADERS_IDX,
                    stbRecordStride,
                    SWS_PRIMARY_MISS_SHADERS_IDX,
                    origin,
                    tmin,
                    direction,
                    tmax,
                    SWS_LOC_PRIMARY_RAY);
        const vec3 hitColor = PrimaryRay.colorAndDist.rgb * pow(0.7f, i);
        const float hitDistance = PrimaryRay.colorAndDist.w;
        cullMask = 0x01;
        if (hitDistance < 0.0f) 
        {
            // // 如果一开始就打到背景，直接给背景颜色
            // if (i == 0)
            // {
            //     finalColor += hitColor;
                
            // }
            // // 如果是经过二次射线碰撞打到的背景，就减弱背景色加入到当前颜色中
            // else
            // {
            //     finalColor += hitColor * 0.3f;
            // }
            // break;
            // finalColor = vec3(0.5, 0.5, 0.5);
            finalColor += hitColor;
            break;
        } 
        else
        {
            // 如果击中物体
            const vec3 hitNormal = PrimaryRay.normalAndObjId.xyz;
            const float objectId = PrimaryRay.normalAndObjId.w;

            const vec3 hitPos = origin + direction * hitDistance;

            bool needsReflection = ObjAttris[int(objectId)].objAttri.reflection > 0;
            origin = hitPos + hitNormal * 0.01f;
            direction = reflect(direction, hitNormal);
            // if (int(objectId) == 1 || int(objectId) == 17 || int(objectId) == 13)
            // if (ObjAttris[int(objectId)].objAttri.reflection > 0)
            // {
            //     needsReflection = true;
            //     // if (int(objectId) == 17 || int(objectId) == 13)
            // }            
            if (ObjAttris[int(objectId)].objAttri.refraction > 0)
            {
                if (i == 0)
                {
                    vec3 refractionLight = refract(direction, hitNormal, 1.1f);
                    vec3 refractionOrigin = hitPos - hitNormal * 0.1f;
                    traceRayEXT(Scene,
                        rayFlags,
                        cullMask,
                        SWS_PRIMARY_HIT_SHADERS_IDX,
                        stbRecordStride,
                        SWS_PRIMARY_MISS_SHADERS_IDX,
                        refractionOrigin,
                        tmin,
                        refractionLight,
                        tmax,
                        SWS_LOC_PRIMARY_RAY);
                        if (hitDistance > 0.0f)
                        {
                            finalColor += PrimaryRay.colorAndDist.rgb * 0.3f;
                        }
                }
            }
            const vec3 toLight = normalize(Params.sunPosAndAmbient.xyz);
            const vec3 shadowRayOrigin = hitPos + hitNormal * 0.001f;
            traceRayEXT(Scene,
                shadowRayFlags,
                0xFD,
                SWS_SHADOW_HIT_SHADERS_IDX,
                stbRecordStride,
                SWS_SHADOW_MISS_SHADERS_IDX,
                shadowRayOrigin,
                0.0f,
                toLight,
                tmax,
                SWS_LOC_SHADOW_RAY);
            const float lighting = (ShadowRay.distance > 0.0f) ? Params.sunPosAndAmbient.w : max(Params.sunPosAndAmbient.w, dot(hitNormal, toLight));
            finalColor += hitColor * lighting;
            if (!needsReflection)
            {
                break;
            }
        }
    }

    imageStore(ResultImage, ivec2(gl_LaunchIDEXT.xy), vec4(finalColor, 1.0f));
}
