"""Temple interior materials (Claude, 2026-10-20; docs/CHULT-TEMPLE-INTERIOR.md).

M_TempleDungeonStone: the ruins' CC0 scans already imported by import_chult_ruins.py
(rock_wall_08, rabdentse_ruins_wall, mossy_stone_wall in /Game/Art/ChultRuins), projected
world-aligned exactly as M_RuinStone is, but as dry, darker, warm-grey limestone with
sparse damp stains and only a trace of moss on upward faces. M_RuinStone grows moss
below 5 m of world height, which is where the whole interior lies.
M_TempleVoid: unlit black, for the grate holes, cracks and carved throats (M_Dark is
lit and reads grey under the torches).

Creates or rebuilds only these two assets in /Game/Art/ChultTemple; imports nothing.
UnrealEditor-Cmd <project> -ExecutePythonScript=Tools/create_temple_interior_materials.py -unattended -nosplash -NoLiveCoding
"""
from pathlib import Path
import unreal

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
DEST = '/Game/Art/ChultTemple'
SCANS = '/Game/Art/ChultRuins'
helper = Path(__file__).with_name('import_fall_cutscene.py')
scope = {'__file__': str(helper), '__name__': 'temple_material_helpers'}
exec(compile(helper.read_text(encoding='utf-8').split('# Music:')[0], str(helper), 'exec'), scope)
node, const, colour, mul, lerp, out, link, mask = [scope[k] for k in ('node', 'const', 'colour', 'mul', 'lerp', 'out', 'link', 'mask')]


def custom(m, code, inputs, kind=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    args = []
    for name in inputs:
        i = unreal.CustomInput(); i.set_editor_property('input_name', name); args.append(i)
    c = node(m, unreal.MaterialExpressionCustom, code=code, inputs=args, output_type=kind)
    for name, expr in inputs.items(): link(expr, c, name)
    return c


# The same triplanar projection as M_RuinStone (import_chult_ruins.py).
SAMPLE = r'''
float3 n=normalize(N),s=sign(n),w=pow(abs(n),4);w/=max(dot(w,float3(1,1,1)),.0001);
float2 ux=P.yz*float2(s.x,1)/360.,uy=P.xz*float2(-s.y,1)/360.,uz=P.xy*float2(s.z,1)/360.;
float3 x=Texture2DSample(Tex,TexSampler,ux).rgb,y=Texture2DSample(Tex,TexSampler,uy).rgb,z=Texture2DSample(Tex,TexSampler,uz).rgb;
if(NORMAL) {
float2 nx=x.rg*2-1,ny=y.rg*2-1,nz=z.rg*2-1;
float3 perturb=float3(0,nx.x*s.x,nx.y)*w.x+float3(-ny.x*s.y,0,ny.y)*w.y+float3(nz.x*s.z,nz.y,0)*w.z;
perturb-=n*dot(n,perturb);return normalize(n+perturb*.5);
}return x*w.x+y*w.y+z*w.z;
'''


def material(name):
    m = unreal.load_asset(DEST + '/' + name) or TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    LIB.delete_all_material_expressions(m)
    return m


def stone():
    m = material('M_TempleDungeonStone')
    m.set_editor_property('tangent_space_normal', False)
    p = node(m, unreal.MaterialExpressionWorldPosition); n = node(m, unreal.MaterialExpressionVertexNormalWS)
    maps = {}
    for asset in ('rock_wall_08', 'mossy_stone_wall', 'rabdentse_ruins_wall'):
        for role in ('diff', 'arm', 'nor_dx'):
            tex = unreal.load_asset(f'{SCANS}/T_{asset}_{role}_2k')
            if not tex: raise RuntimeError('Missing scan ' + asset + ' ' + role)
            typ = {'diff': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, 'arm': unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 'nor_dx': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL}[role]
            obj = node(m, unreal.MaterialExpressionTextureObject, texture=tex, sampler_type=typ)
            maps[asset, role] = custom(m, SAMPLE.replace('NORMAL', '1' if role == 'nor_dx' else '0'), {'Tex': obj, 'P': p, 'N': n})
    noise = node(m, unreal.MaterialExpressionNoise, scale=.011, levels=3, quality=1, output_min=0., output_max=1.); link(p, noise, '')
    fine = node(m, unreal.MaterialExpressionNoise, scale=.13, levels=2, quality=1, output_min=0., output_max=1.); link(p, fine, '')
    wear = custom(m, 'return smoothstep(.25,.8,V)*.5;', {'V': noise}, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    base = lerp(m, maps['rock_wall_08', 'diff'], maps['rabdentse_ruins_wall', 'diff'], wear)
    # Dry limestone: desaturated to a warm grey, darker than the sunlit ruins; damp stains darken it in patches.
    base = custom(m, '''float l=dot(S,float3(.2126,.7152,.0722));float3 c=lerp(S,l*float3(1.0,.94,.84),.86)*.6;
float damp=smoothstep(.5,.95,V);return c*(1-.22*damp);''', {'S': base, 'V': noise})
    # A trace of moss, only on upward faces, in rare patches.
    coverage = custom(m, 'return smoothstep(.84,.97,V+F*.06)*saturate(N.z*1.5)*.7;', {'V': noise, 'F': fine, 'N': n}, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    moss = custom(m, 'float t=dot(S,float3(.21,.72,.07));return lerp(float3(.018,.026,.01),float3(.06,.08,.035),saturate(t*2.2));', {'S': maps['mossy_stone_wall', 'diff']})
    out(m, lerp(m, base, moss, coverage), unreal.MaterialProperty.MP_BASE_COLOR)
    mixedn = lerp(m, lerp(m, maps['rock_wall_08', 'nor_dx'], maps['rabdentse_ruins_wall', 'nor_dx'], wear), maps['mossy_stone_wall', 'nor_dx'], mul(m, coverage, const(m, .5)))
    norm = node(m, unreal.MaterialExpressionNormalize); link(mixedn, norm, ''); out(m, norm, unreal.MaterialProperty.MP_NORMAL)
    arm = lerp(m, maps['rock_wall_08', 'arm'], maps['rabdentse_ruins_wall', 'arm'], wear)
    out(m, mask(m, arm, r=True), unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    rough = custom(m, 'return lerp(clamp(R,.55,.9),.95,M);', {'R': mask(m, arm, g=True), 'M': coverage}, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    out(m, rough, unreal.MaterialProperty.MP_ROUGHNESS); out(m, const(m, .22), unreal.MaterialProperty.MP_SPECULAR)
    LIB.recompile_material(m); ASSETS.save_loaded_asset(m, only_if_is_dirty=False)
    unreal.log('CHUCK_TEMPLE_MATERIAL M_TempleDungeonStone world_aligned=1 height_moss=0')


def void():
    m = material('M_TempleVoid')
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    out(m, colour(m, 0, 0, 0), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.recompile_material(m); ASSETS.save_loaded_asset(m, only_if_is_dirty=False)
    unreal.log('CHUCK_TEMPLE_MATERIAL M_TempleVoid unlit=1')


stone(); void()
unreal.log('CHUCK_TEMPLE_MATERIALS_DONE materials=2')
