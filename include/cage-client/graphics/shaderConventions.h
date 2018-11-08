#define CAGE_SHADER_MAX_INSTANCES 128
#define CAGE_SHADER_MAX_BONES 1365

// attribute in locations

#define CAGE_SHADER_ATTRIB_IN_POSITION 0
#define CAGE_SHADER_ATTRIB_IN_UV 1
#define CAGE_SHADER_ATTRIB_IN_NORMAL 2
#define CAGE_SHADER_ATTRIB_IN_TANGENT 3
#define CAGE_SHADER_ATTRIB_IN_BITANGENT 4
#define CAGE_SHADER_ATTRIB_IN_BONEINDEX 5
#define CAGE_SHADER_ATTRIB_IN_BONEWEIGHT 6

// attribute out locations

#define CAGE_SHADER_ATTRIB_OUT_ALBEDO 0
#define CAGE_SHADER_ATTRIB_OUT_SPECIAL 1
#define CAGE_SHADER_ATTRIB_OUT_NORMAL 2
#define CAGE_SHADER_ATTRIB_OUT_COLOR 3

// texture sampler bindings

#define CAGE_SHADER_TEXTURE_ALBEDO 0
#define CAGE_SHADER_TEXTURE_SPECIAL 1
#define CAGE_SHADER_TEXTURE_NORMAL 2
#define CAGE_SHADER_TEXTURE_USER 3
#define CAGE_SHADER_TEXTURE_ALBEDO_ARRAY 5
#define CAGE_SHADER_TEXTURE_SPECIAL_ARRAY 6
#define CAGE_SHADER_TEXTURE_NORMAL_ARRAY 7
#define CAGE_SHADER_TEXTURE_USER_ARRAY 8
#define CAGE_SHADER_TEXTURE_SHADOW 10
#define CAGE_SHADER_TEXTURE_SHADOW_CUBE 11
#define CAGE_SHADER_TEXTURE_DEPTH 12

// uniform locations

#define CAGE_SHADER_UNI_BONESPERINSTANCE 1

// uniform block bindings

#define CAGE_SHADER_UNIBLOCK_VIEWPORT 0
#define CAGE_SHADER_UNIBLOCK_MESHES 1
#define CAGE_SHADER_UNIBLOCK_MATERIAL 2
#define CAGE_SHADER_UNIBLOCK_ARMATURES 3
#define CAGE_SHADER_UNIBLOCK_LIGHTS 4

// subroutine procedure indexes

#define CAGE_SHADER_ROUTINEPROC_MATERIALNOTHING 1
#define CAGE_SHADER_ROUTINEPROC_MAPALBEDO2D 2
#define CAGE_SHADER_ROUTINEPROC_MAPALBEDOARRAY 3
#define CAGE_SHADER_ROUTINEPROC_MAPSPECIAL2D 4
#define CAGE_SHADER_ROUTINEPROC_MAPSPECIALARRAY 5
#define CAGE_SHADER_ROUTINEPROC_MAPNORMAL2D 6
#define CAGE_SHADER_ROUTINEPROC_MAPNORMALARRAY 7
#define CAGE_SHADER_ROUTINEPROC_SKELETONNOTHING 11
#define CAGE_SHADER_ROUTINEPROC_SKELETONANIMATION 12
#define CAGE_SHADER_ROUTINEPROC_LIGHTDIRECTIONAL 21
#define CAGE_SHADER_ROUTINEPROC_LIGHTDIRECTIONALSHADOW 22
#define CAGE_SHADER_ROUTINEPROC_LIGHTSPOT 23
#define CAGE_SHADER_ROUTINEPROC_LIGHTSPOTSHADOW 24
#define CAGE_SHADER_ROUTINEPROC_LIGHTPOINT 25
#define CAGE_SHADER_ROUTINEPROC_LIGHTPOINTSHADOW 26
#define CAGE_SHADER_ROUTINEPROC_LIGHTAMBIENT 27

// subroutine uniform locations

#define CAGE_SHADER_ROUTINEUNIF_SKELETON 0
#define CAGE_SHADER_ROUTINEUNIF_MAPALBEDO 1
#define CAGE_SHADER_ROUTINEUNIF_MAPSPECIAL 2
#define CAGE_SHADER_ROUTINEUNIF_MAPNORMAL 3
#define CAGE_SHADER_ROUTINEUNIF_LIGHTTYPE 4
