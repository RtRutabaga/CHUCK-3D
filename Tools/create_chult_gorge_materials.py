"""The Chult gorge's textures and two materials (Claude, 2026-10-15).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/create_chult_gorge_materials.py -unattended -nosplash -NoLiveCoding

Imports the nine verified CC0 Poly Haven maps fetched by Tools/Fetch-ChultGorge.ps1
(SourceAssets/Setting/ChultGorge) into /Game/Art/ChultGorge and builds:

  M_ChultGorge   opaque landscape/canyon surface. Vertex colour R is the rock
                 weight (0 jungle, 1 bare rock), G pushes the rock from the
                 stratified cliff towards mossy crag, B is spray wetness. The
                 rock/jungle edge is broken by the textures' own relief, so no
                 face ever switches material on a triangle edge. Triplanar
                 rock on the procedural meshes' local position (small numbers,
                 no large-world precision loss); jungle seen from far away
                 turns into mottled canopy.
  M_ChultPlunge  translucent free-falling water. UV is world centimetres/200
                 (U across, V down the fall); vertex colour R is the foam
                 amount (glassy crest -> broken white water), G the normalised
                 position across the sheet, A the opacity.

No existing material, mesh or texture is changed.
"""
from pathlib import Path
import hashlib
import json
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Setting/ChultGorge'
DEST = '/Game/Art/ChultGorge'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
LIB = unreal.MaterialEditingLibrary
helper = Path(__file__).with_name('import_fall_cutscene.py')
scope = {'__file__': str(helper), '__name__': 'gorge_helpers'}
exec(compile(helper.read_text(encoding='utf-8').split('# Music:')[0], str(helper), 'exec'), scope)
node, const, colour, out, link = [scope[k] for k in ('node', 'const', 'colour', 'out', 'link')]
ASSETS.make_directory(DEST)

textures = {}
for rec in json.loads((SOURCE / 'manifest.json').read_text(encoding='utf-8-sig'))['assets']:
    file = SOURCE / rec['file']
    if hashlib.sha256(file.read_bytes()).hexdigest().upper() != rec['sha256']:
        raise RuntimeError(file.name + ' hash')
    name = 'T_' + file.stem
    task = unreal.AssetImportTask(); task.filename = str(file); task.destination_path = DEST
    task.destination_name = name; task.automated = True; task.replace_existing = True; task.save = False
    TOOLS.import_asset_tasks([task]); tex = unreal.load_asset(DEST + '/' + name)
    role = rec['role'].lower()
    tex.set_editor_property('srgb', role == 'diffuse')
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP if role == 'nor_dx' else
                            unreal.TextureCompressionSettings.TC_DEFAULT if role == 'diffuse' else unreal.TextureCompressionSettings.TC_MASKS)
    ASSETS.save_loaded_asset(tex, only_if_is_dirty=False)
    textures[rec['asset'], role] = tex


def custom(m, code, inputs, kind=unreal.CustomMaterialOutputType.CMOT_FLOAT3, extra=()):
    args = []
    for name in inputs:
        i = unreal.CustomInput(); i.set_editor_property('input_name', name); args.append(i)
    outs = []
    for name, typ in extra:
        o = unreal.CustomOutput(); o.set_editor_property('output_name', name); o.set_editor_property('output_type', typ); outs.append(o)
    c = node(m, unreal.MaterialExpressionCustom, code=code, inputs=args, output_type=kind, additional_outputs=outs)
    for name, expr in inputs.items():
        if isinstance(expr, tuple): link(expr[0], c, name, expr[1])
        else: link(expr, c, name)
    return c


def tex_object(m, asset, role):
    typ = {'diffuse': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, 'nor_dx': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
           'arm': unreal.MaterialSamplerType.SAMPLERTYPE_MASKS}[role]
    return node(m, unreal.MaterialExpressionTextureObject, texture=textures[asset, role], sampler_type=typ)


def material(name, build):
    path = f'{DEST}/{name}'
    mat = unreal.load_asset(path)
    if mat:
        LIB.delete_all_material_expressions(mat)
    else:
        mat = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    build(mat)
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    unreal.log(f'CHUCK_GORGE_MATERIAL {name}')


GORGE = r'''
struct GorgeFns
{
    float3 Tri(Texture2D T,SamplerState S,float3 p,float s,float3 sg,float3 w)
    {
        float3 x=Texture2DSample(T,S,p.yz*float2(sg.x,1)/s).rgb,y=Texture2DSample(T,S,p.xz*float2(-sg.y,1)/s).rgb,z=Texture2DSample(T,S,p.xy*float2(sg.z,1)/s).rgb;
        return x*w.x+y*w.y+z*w.z;
    }
    float3 TriN(Texture2D T,SamplerState S,float3 p,float s,float3 sg,float3 w,float3 n)
    {
        float2 x=Texture2DSample(T,S,p.yz*float2(sg.x,1)/s).rg*2-1,y=Texture2DSample(T,S,p.xz*float2(-sg.y,1)/s).rg*2-1,z=Texture2DSample(T,S,p.xy*float2(sg.z,1)/s).rg*2-1;
        float3 d=float3(0,x.x*sg.x,x.y)*w.x+float3(-y.x*sg.y,0,y.y)*w.y+float3(z.x*sg.z,z.y,0)*w.z;
        return d-n*dot(n,d);
    }
};
GorgeFns F;
float3 n=normalize(N);
float3 sg=float3(n.x>=0?1:-1,n.y>=0?1:-1,n.z>=0?1:-1);
float3 w=pow(abs(n),5);w/=max(dot(w,float3(1,1,1)),.0001);
// Large scale variation, sampled once from above.
float3 macro=Texture2DSample(GD,GDSampler,P.xy/52000).rgb;float mv=dot(macro,float3(.3,.59,.11));
float3 macro2=Texture2DSample(RD,RDSampler,P.xy/17000+.37).rgb;float mv2=dot(macro2,float3(.3,.59,.11));
// Rock: the stratified cliff, giving way to mossy crag lower down and in patches.
float3 strata=F.Tri(CD,CDSampler,P,1700,sg,w);
float3 crag=F.Tri(RD,RDSampler,P,950,sg,w);
float cragW=saturate(VC.g*1.1+(mv2-.42)*2.2);
float sl=dot(strata,float3(.3,.59,.11));
float3 bedded=lerp(strata,sl*float3(.93,.88,.79),.58)*1.04;
float3 rock=lerp(bedded,crag*float3(.86,.92,.8),cragW);
// Damp jungle grime greys and greens all exposed rock.
rock=lerp(rock,rock*float3(.76,.95,.62),.35);
float rockLum=dot(rock,float3(.3,.59,.11));
// Dark wet streaks run down vertical faces; spray soaks the rock by the falls.
float streak=Texture2DSample(RD,RDSampler,float2((P.x*.6+P.y*.8)/2600,P.z/16000)).g;
float wet=saturate(VC.b+smoothstep(.42,.72,streak)*.55*(1-abs(n.z)));
rock*=lerp(1,.5,wet);
// Moss on rock that faces the sky.
float moss=smoothstep(.35,.9,n.z+(mv-.45)*.8)*(1-VC.b*.5);
rock=lerp(rock,float3(.05,.08,.025)*(.7+mv*.8),moss*.75);
// Jungle: undergrowth close up, mottled canopy from afar.
float3 grass=F.Tri(GD,GDSampler,P,650,sg,w);
float gl=dot(grass,float3(.3,.59,.11));
float3 under=grass*float3(.46,.74,.32)*(.8+mv*.5);
float crowns=Texture2DSample(GD,GDSampler,P.xy/2300+.21).g,stands=Texture2DSample(RD,RDSampler,P.xy/9000+.6).g;
float3 canopy=lerp(float3(.022,.05,.014),float3(.085,.15,.035),saturate(crowns*.8+stands*.5-.15))*lerp(float3(1.15,1.02,.7),float3(.85,1.04,1.04),mv);
float far=saturate((D-4500)/26000);
float3 veg=lerp(under,canopy,far);
// The edge between them follows the textures' own relief, never a triangle.
float r=saturate((VC.r-.5)*3.2+(rockLum-gl)*1.4+.5);
r=saturate(r+saturate((.38-n.z)*2.6)*(1-far*.6));
float3 Nr=F.TriN(CN,CNSampler,P,1700,sg,w,n)*(1-cragW)+F.TriN(RN,RNSampler,P,950,sg,w,n)*cragW;
float3 Ng=F.TriN(GN,GNSampler,P,650,sg,w,n);
WN=normalize(n+lerp(Ng*.5*(1-far),Nr*.85,r));
float ao=F.Tri(CA,CASampler,P,1700,sg,w).r;
AO=lerp(1,ao,r*(1-cragW*.5));
Rough=lerp(.93,lerp(.86,.42,wet),r);
return lerp(veg,rock,r);
'''


def gorge(m):
    m.set_editor_property('tangent_space_normal', False)
    p = node(m, unreal.MaterialExpressionLocalPosition)
    n = node(m, unreal.MaterialExpressionVertexNormalWS)
    vc = node(m, unreal.MaterialExpressionVertexColor)
    d = node(m, unreal.MaterialExpressionPixelDepth)
    inputs = {'P': p, 'N': n, 'VC': vc, 'D': d}
    for key, asset, role in [('CD', 'cliff_side', 'diffuse'), ('CN', 'cliff_side', 'nor_dx'), ('CA', 'cliff_side', 'arm'),
                             ('RD', 'aerial_rocks_02', 'diffuse'), ('RN', 'aerial_rocks_02', 'nor_dx'),
                             ('GD', 'aerial_grass_rock', 'diffuse'), ('GN', 'aerial_grass_rock', 'nor_dx')]:
        inputs[key] = tex_object(m, asset, role)
    c = custom(m, GORGE, inputs, extra=[('WN', unreal.CustomMaterialOutputType.CMOT_FLOAT3),
                                       ('AO', unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                                       ('Rough', unreal.CustomMaterialOutputType.CMOT_FLOAT1)])
    out(m, c, unreal.MaterialProperty.MP_BASE_COLOR)
    out(m, c, unreal.MaterialProperty.MP_NORMAL, 'WN')
    out(m, c, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, 'AO')
    out(m, c, unreal.MaterialProperty.MP_ROUGHNESS, 'Rough')
    out(m, const(m, .3), unreal.MaterialProperty.MP_SPECULAR)


PLUNGE = r'''
struct PlungeFns
{
    float H(float2 i){return frac(sin(dot(i,float2(127.1,311.7)))*43758.5453);}
    float N(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(H(i),H(i+float2(1,0)),f.x),lerp(H(i+float2(0,1)),H(i+1),f.x),f.y);}
    float Fbm(float2 p){return N(p)*.5+N(p*2.03+7.1)*.27+N(p*4.1+3.3)*.15+N(p*8.3+1.7)*.08;}
};
PlungeFns F;
float foam=saturate(VC.r),across=VC.g;
// Long streaks stretched down the flow at two speeds; finer, faster lace on top.
float a=F.Fbm(float2(UV.x*3.1,UV.y*.32-T*1.35));
float b=F.Fbm(float2(UV.x*7.3+11.,UV.y*.75-T*2.4));
float c=F.Fbm(float2(UV.x*16.,UV.y*1.9-T*3.7));
float streak=saturate(a*.8+b*.55+c*.3-.38);
float edge=smoothstep(0,.16+.1*b,across)*smoothstep(0,.16+.1*a,1-across);
// The crest is glassy and continuous; lower down it breaks into white lace.
float glass=.62+.3*a;
float lace=saturate(streak*1.45-.12)*(.55+.45*foam);
Opacity=saturate(lerp(glass,lace,foam)*edge*VA);
Emit=lerp(float3(.03,.05,.05),float3(.2,.22,.22),lerp(.25,1,foam)*streak);
return lerp(float3(.09,.15,.14),float3(.82,.87,.86),saturate(foam*.7+streak*.6));
'''


def plunge(m):
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    m.set_editor_property('two_sided', True)
    uv = node(m, unreal.MaterialExpressionTextureCoordinate)
    t = node(m, unreal.MaterialExpressionTime)
    vc = node(m, unreal.MaterialExpressionVertexColor)
    c = custom(m, PLUNGE, {'UV': uv, 'T': t, 'VC': vc, 'VA': (vc, 'A')},
               extra=[('Opacity', unreal.CustomMaterialOutputType.CMOT_FLOAT1), ('Emit', unreal.CustomMaterialOutputType.CMOT_FLOAT3)])
    fade = node(m, unreal.MaterialExpressionDepthFade, fade_distance_default=90.)
    link(c, fade, 'Opacity', 'Opacity')
    out(m, c, unreal.MaterialProperty.MP_BASE_COLOR)
    out(m, fade, unreal.MaterialProperty.MP_OPACITY)
    out(m, c, unreal.MaterialProperty.MP_EMISSIVE_COLOR, 'Emit')
    out(m, const(m, .12), unreal.MaterialProperty.MP_ROUGHNESS)
    out(m, const(m, .5), unreal.MaterialProperty.MP_SPECULAR)


material('M_ChultGorge', gorge)
material('M_ChultPlunge', plunge)
unreal.log(f'CHUCK_GORGE_MATERIALS_READY materials=2 textures={len(textures)}')
