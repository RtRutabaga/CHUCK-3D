"""Phlegethos's textures, scores and materials (Claude, 2026-11-14; docs/regions/PHLEGETHOS.md).

UnrealEditor-Cmd <uproject> -run=pythonscript -script=Tools/create_phlegethos_assets.py -AllowCommandletAudio -unattended -nosplash

Imports the twelve CC0 maps fetched by Tools/Fetch-Phlegethos.ps1 (SourceAssets/Setting/Phlegethos) into
/Game/Art/Phlegethos, the user's two Phlegethos tracks (SourceAssets/Audio/Phlegethos.wav, Phlegethos2.wav, unmodified)
into /Game/Art/Audio as SW_Phlegethos and SW_Phlegethos2 (looping), and builds:

  M_PhlegBasalt  opaque basalt for the procedural stage (triplanar on local position, which is the stage frame: Z is the
                 height above the lava). Vertex colour R: cracked heat (glowing seams in the rock's dark creases), G: the
                 second basalt, B: ash settled on top. The lava's light comes up the faces near it (Z under about 2 m).
  M_PhlegPath    the worn volcanic flagstones of the road and the fortress, with ash in the joints.
  M_PhlegLava    the molten surface: the lava scan, its crust dark and its cracks white-hot, drifting and churning; UV V
                 runs down a flow (rivers, falls) and vertex colour G is its speed. Strongly emissive.
  M_PhlegGlow    plain emissive for fissures, eyes and the lava bombs' marks; vertex colour RGB is the colour, A pulses.
  M_PhlegEmber   additive camera-facing embers and ash (PerInstanceCustomData 0: heat 0 ash .. 1 white-hot).

Run once after fetching; reruns rebuild only these assets. Existing materials are not touched.
"""
from pathlib import Path
import hashlib
import json
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Setting/Phlegethos'
DEST = '/Game/Art/Phlegethos'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
LIB = unreal.MaterialEditingLibrary
ASSETS.make_directory(DEST)


def node(mat, cls, **props):
    n = LIB.create_material_expression(mat, cls)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


def link(a, b, pin_b, pin_a=''):
    if not LIB.connect_material_expressions(a, pin_a, b, pin_b):
        raise RuntimeError(f'Material link failed: {pin_a} -> {pin_b}')


def out(mat, expr, prop, pin=''):
    if not LIB.connect_material_property(expr, pin, prop):
        raise RuntimeError(f'Material output failed: {prop}')


def const(mat, v):
    return node(mat, unreal.MaterialExpressionConstant, r=v)


# ---------------------------------------------------------------- music
for source, name in (('Phlegethos.wav', 'SW_Phlegethos'), ('Phlegethos2.wav', 'SW_Phlegethos2')):
    task = unreal.AssetImportTask()
    task.filename = str(ROOT / 'SourceAssets/Audio' / source); task.destination_path = '/Game/Art/Audio'; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = True
    TOOLS.import_asset_tasks([task])
    wave = unreal.load_asset('/Game/Art/Audio/' + name)
    if not isinstance(wave, unreal.SoundWave): raise RuntimeError(name + ' import failed')
    wave.set_editor_property('looping', True); wave.set_editor_property('volume', 1.0)
    if not ASSETS.save_loaded_asset(wave, only_if_is_dirty=False): raise RuntimeError(name + ' save failed')
    unreal.log(f'CHUCK_AUDIO_IMPORTED {name} looping=True duration={wave.get_editor_property("duration"):.2f}')

# ---------------------------------------------------------------- textures
textures = {}
for rec in json.loads((SOURCE / 'manifest.json').read_text(encoding='utf-8-sig'))['assets']:
    file = SOURCE / rec['file']
    if hashlib.sha256(file.read_bytes()).hexdigest().upper() != rec['sha256']:
        raise RuntimeError(file.name + ' hash')
    name = 'T_' + file.stem.replace('-', '_')
    task = unreal.AssetImportTask(); task.filename = str(file); task.destination_path = DEST
    task.destination_name = name; task.automated = True; task.replace_existing = True; task.save = False
    TOOLS.import_asset_tasks([task]); tex = unreal.load_asset(DEST + '/' + name)
    role = rec['role'].lower()
    tex.set_editor_property('srgb', role == 'diffuse')
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP if role == 'nor_dx' else
                            unreal.TextureCompressionSettings.TC_DEFAULT if role == 'diffuse' else unreal.TextureCompressionSettings.TC_MASKS)
    ASSETS.save_loaded_asset(tex, only_if_is_dirty=False)
    textures[rec['asset'], role] = tex


def tex_object(m, asset, role):
    typ = {'diffuse': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, 'nor_dx': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
           'arm': unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 'rough': unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE}[role]
    return node(m, unreal.MaterialExpressionTextureObject, texture=textures[asset, role], sampler_type=typ)


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


def material(name, build):
    path = f'{DEST}/{name}'
    mat = unreal.load_asset(path)
    if mat: LIB.delete_all_material_expressions(mat)
    else: mat = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    build(mat)
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    unreal.log(f'CHUCK_PHLEG_MATERIAL {name}')


F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3

TRI = r'''
struct TriFns
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
    float H(float2 i){return frac(sin(dot(i,float2(127.1,311.7)))*43758.5453);}
    float N(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(H(i),H(i+float2(1,0)),f.x),lerp(H(i+float2(0,1)),H(i+1),f.x),f.y);}
};
TriFns F;
float3 n=normalize(N);
float3 sg=float3(n.x>=0?1:-1,n.y>=0?1:-1,n.z>=0?1:-1);
float3 w=pow(abs(n),4);w/=max(dot(w,float3(1,1,1)),.0001);
'''

BASALT = TRI + r'''
// Two basalts, blended in broad patches and by the vertex colour.
float patch=F.N(P.xy/900)*.6+F.N(P.xy/260+3.1)*.4;
float b2=saturate(VC.g+(patch-.5)*1.6);
float3 a=F.Tri(AD,ADSampler,P,260,sg,w),b=F.Tri(BD,BDSampler,P,330,sg,w);
float3 rock=lerp(a,b,b2)*float3(.86,.84,.86);
float lum=dot(rock,float3(.3,.59,.11));
float3 Na=F.TriN(AN,ANSampler,P,260,sg,w,n),Nb=F.TriN(BN,BNSampler,P,330,sg,w,n);
float3 arm=lerp(F.Tri(AA,AASampler,P,260,sg,w),F.Tri(BA,BASampler,P,330,sg,w),b2);
// Ash on whatever faces the sky, thicker in the hollows.
float ash=saturate(VC.b*1.3+smoothstep(.55,.95,n.z)*(.25+.5*F.N(P.xy/180)))*smoothstep(.2,.6,n.z);
float3 ashc=float3(.19,.18,.175)*(.8+.4*F.N(P.xy/40));
rock=lerp(rock,ashc,ash*.75);
// Seams of heat: the deepest creases (the scan's own occlusion, not its colour: this basalt is dark all over) glow where
// the vertex colour says the rock is cracked through.
float crease=saturate((.62-arm.r)*4.)*saturate(1-ash);
float flick=.75+.25*sin(T*1.7+P.x*.013+P.y*.017);
float seam=crease*VC.r;
// The lava's light thrown up the faces close over it.
float under=saturate(1-P.z/240)*saturate(1.2-n.z);
Emit=float3(1.,.28,.04)*(seam*seam*22*flick)+float3(.9,.25,.05)*under*under*.9*(.6+.4*lum*3);
WN=normalize(n+lerp(Na,Nb,b2)*(1-ash*.6)*1.15);
AO=arm.r; Rough=lerp(lerp(.82,.95,arm.g),.97,ash);
return rock;
'''

PATH = TRI + r'''
float3 c=F.Tri(PD,PDSampler,P,180,sg,w)*float3(.9,.86,.84);
float lum=dot(c,float3(.3,.59,.11));
float3 Nn=F.TriN(PN,PNSampler,P,180,sg,w,n);
float3 arm=F.Tri(PA,PASampler,P,180,sg,w);
// Ash lies in the joints and drifts over the worn stones in patches.
float joint=saturate((.11-lum)*9.);
float drift=smoothstep(.45,.85,F.N(P.xy/420)+VC.b*.6)*smoothstep(.4,.8,n.z);
float ash=saturate(joint*.8+drift*.6);
c=lerp(c,float3(.2,.19,.18)*(.8+.4*F.N(P.xy/30)),ash*.8);
float under=saturate(1-P.z/240)*saturate(1.2-n.z);
Emit=float3(.9,.25,.05)*under*under*.9+float3(1,.3,.05)*saturate(VC.r*joint*2)*10;
WN=normalize(n+Nn*(1-ash*.5));
AO=arm.r; Rough=lerp(.78,.95,ash);
return c;
'''

LAVA = r'''
struct LavaFns
{
    float H(float2 i){return frac(sin(dot(i,float2(127.1,311.7)))*43758.5453);}
    float N(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(H(i),H(i+float2(1,0)),f.x),lerp(H(i+float2(0,1)),H(i+1),f.x),f.y);}
};
LavaFns F;
// World-scale UVs (cm/400): drifting slowly, the flow's speed down V (vertex colour G, 0 still .. 1 a fall).
float speed=VC.g;
float2 uv=UV;
float2 warp=float2(F.N(uv*1.3+T*.05),F.N(uv*1.3+5.2-T*.04))-.5;
float2 u1=uv*.55+warp*.35+float2(T*.006,-T*(.012+speed*.9));
float2 u2=uv*1.1-warp*.25+float2(-T*.009,-T*(.02+speed*1.4))+.37;
float3 c1=Texture2DSample(LD,LDSampler,u1).rgb,c2=Texture2DSample(LD,LDSampler,u2).rgb;
float l1=dot(c1,float3(.3,.59,.11)),l2=dot(c2,float3(.3,.59,.11));
// Crust where the scan is dark, molten where bright; the two layers fight, so the cracks open and close.
float molten=saturate(max(l1,l2*.9)*1.6-.42+ (F.N(uv*2.5+T*.15)-.5)*.35);
molten=saturate(molten+speed*.35);
// Far off, the crust's cracks merge into a glow: the lake seen from the fall or across it reads molten, not as sand.
molten=saturate(molten+saturate((D-1500)/25000)*.55);
float pulse=.85+.15*sin(T*1.3+uv.x*2.1+uv.y*1.7);
float3 hot=lerp(float3(1.,.18,.02),float3(1.,.62,.18),molten*molten)*pulse;
Emit=hot*molten*(14+30*molten*molten)*VA;
Rough=lerp(.55,.25,molten);
float3 Nn=Texture2DSample(LN,LNSampler,u1).rgb*2-1;
WN=normalize(float3(Nn.xy*(1-molten)*.8,1));
return lerp(float3(.035,.025,.022),float3(.25,.06,.01),molten);
'''

GLOW = r'''
float p=lerp(1,.6+.4*sin(T*2.3+P.x*.02+P.y*.03),VA);
return VC.rgb*p*S;
'''

EMBER = r'''
float2 d=UV-.5; float r=saturate(1-length(d)*2);
float heat=CD.x;
float core=pow(r,lerp(3.,1.6,heat));
float3 hot=lerp(float3(.08,.07,.065),lerp(float3(1.,.22,.03),float3(1.,.8,.4),saturate(heat*1.4-.4)),saturate(heat*3));
return hot*core*lerp(.6,60,heat);
'''


def basalt(m):
    m.set_editor_property('tangent_space_normal', False)
    inputs = {'P': node(m, unreal.MaterialExpressionLocalPosition), 'N': node(m, unreal.MaterialExpressionVertexNormalWS),
              'VC': node(m, unreal.MaterialExpressionVertexColor), 'T': node(m, unreal.MaterialExpressionTime)}
    for key, asset, role in [('AD', 'dark_rock', 'diffuse'), ('AN', 'dark_rock', 'nor_dx'), ('AA', 'dark_rock', 'arm'),
                             ('BD', 'dark_rock_02', 'diffuse'), ('BN', 'dark_rock_02', 'nor_dx'), ('BA', 'dark_rock_02', 'arm')]:
        inputs[key] = tex_object(m, asset, role)
    c = custom(m, BASALT, inputs, extra=[('Emit', F3), ('WN', F3), ('AO', F1), ('Rough', F1)])
    out(m, c, unreal.MaterialProperty.MP_BASE_COLOR)
    out(m, c, unreal.MaterialProperty.MP_EMISSIVE_COLOR, 'Emit')
    out(m, c, unreal.MaterialProperty.MP_NORMAL, 'WN')
    out(m, c, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, 'AO')
    out(m, c, unreal.MaterialProperty.MP_ROUGHNESS, 'Rough')
    out(m, const(m, .35), unreal.MaterialProperty.MP_SPECULAR)


def path(m):
    m.set_editor_property('tangent_space_normal', False)
    inputs = {'P': node(m, unreal.MaterialExpressionLocalPosition), 'N': node(m, unreal.MaterialExpressionVertexNormalWS),
              'VC': node(m, unreal.MaterialExpressionVertexColor)}
    for key, role in [('PD', 'diffuse'), ('PN', 'nor_dx'), ('PA', 'arm')]:
        inputs[key] = tex_object(m, 'volcanic_rock_tiles', role)
    c = custom(m, PATH, inputs, extra=[('Emit', F3), ('WN', F3), ('AO', F1), ('Rough', F1)])
    out(m, c, unreal.MaterialProperty.MP_BASE_COLOR)
    out(m, c, unreal.MaterialProperty.MP_EMISSIVE_COLOR, 'Emit')
    out(m, c, unreal.MaterialProperty.MP_NORMAL, 'WN')
    out(m, c, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, 'AO')
    out(m, c, unreal.MaterialProperty.MP_ROUGHNESS, 'Rough')
    out(m, const(m, .3), unreal.MaterialProperty.MP_SPECULAR)


def lava(m):
    vc = node(m, unreal.MaterialExpressionVertexColor)
    inputs = {'UV': node(m, unreal.MaterialExpressionTextureCoordinate), 'T': node(m, unreal.MaterialExpressionTime),
              'VC': vc, 'VA': (vc, 'A'), 'D': node(m, unreal.MaterialExpressionPixelDepth),
              'LD': tex_object(m, 'Lava004', 'diffuse'), 'LN': tex_object(m, 'Lava004', 'nor_dx')}
    c = custom(m, LAVA, inputs, extra=[('Emit', F3), ('WN', F3), ('Rough', F1)])
    out(m, c, unreal.MaterialProperty.MP_BASE_COLOR)
    out(m, c, unreal.MaterialProperty.MP_EMISSIVE_COLOR, 'Emit')
    out(m, c, unreal.MaterialProperty.MP_NORMAL, 'WN')
    out(m, c, unreal.MaterialProperty.MP_ROUGHNESS, 'Rough')


def glow(m):
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    vc = node(m, unreal.MaterialExpressionVertexColor)
    c = custom(m, GLOW, {'VC': vc, 'VA': (vc, 'A'), 'T': node(m, unreal.MaterialExpressionTime), 'P': node(m, unreal.MaterialExpressionLocalPosition),
                         'S': const(m, 18.)})
    out(m, c, unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def ember(m):
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('used_with_instanced_static_meshes', True)
    cd = node(m, unreal.MaterialExpressionPerInstanceCustomData, data_index=0)
    c = custom(m, EMBER, {'UV': node(m, unreal.MaterialExpressionTextureCoordinate), 'CD': cd})
    out(m, c, unreal.MaterialProperty.MP_EMISSIVE_COLOR)


material('M_PhlegBasalt', basalt)
material('M_PhlegPath', path)
material('M_PhlegLava', lava)
material('M_PhlegGlow', glow)
material('M_PhlegEmber', ember)
unreal.log(f'CHUCK_PHLEG_ASSETS_READY materials=5 textures={len(textures)}')
