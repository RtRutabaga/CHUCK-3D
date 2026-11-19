"""The Feywild's look-slice assets (Claude, 2026-11-18; docs/regions/FEYWILD.md).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=<absolute path to this file> -AllowCommandletAudio -unattended -nosplash -NoLiveCoding

Imports the CC0 maps (Tools/Fetch-Feywild.ps1), the kit (Tools/build_feywild_kit.py) and the user's Feywild_main.wav into
/Game/Art/Feywild and /Game/Art/Audio, and builds the materials. Shares the Chult bark, ground and leaf textures.

With the environment variable CHUCK_FEY_ONLY=M_FeySky[,M_...] it rebuilds only those materials (no imports).
With CHUCK_FEY_MESHES=1 it reimports the kit's meshes as well as the materials named there (CHUCK_FEY_MESH_ONLY=SM_A,SM_B limits which).
With CHUCK_FEY_SOUND=1 it imports the waterfall loop (Tools/gen_feywild_sfx.py) even when CHUCK_FEY_ONLY is set.
"""
from pathlib import Path
import hashlib
import json
import os
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Setting/Feywild'
DEST = '/Game/Art/Feywild'
NATURE = '/Game/Art/ChultNature'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
LIB = unreal.MaterialEditingLibrary
ASSETS.make_directory(DEST)
ONLY = [n.strip() for n in os.environ.get('CHUCK_FEY_ONLY', '').split(',') if n.strip()]
MESHES = not ONLY or os.environ.get('CHUCK_FEY_MESHES') == '1'
MESH_ONLY = [n.strip() for n in os.environ.get('CHUCK_FEY_MESH_ONLY', '').split(',') if n.strip()]
P = unreal.MaterialProperty
F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3


def node(mat, cls, **props):
    n = LIB.create_material_expression(mat, cls)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


def link(a, b, pin_b, pin_a=''):
    if not LIB.connect_material_expressions(a, pin_a, b, pin_b): raise RuntimeError(f'Material link failed: {pin_a} -> {pin_b}')


def out(expr, prop, pin=''):
    if not LIB.connect_material_property(expr, pin, prop): raise RuntimeError(f'Material output failed: {prop}')


def const(mat, v): return node(mat, unreal.MaterialExpressionConstant, r=v)


def custom(m, code, inputs, kind=F3, extra=()):
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
    if ONLY and name not in ONLY: return None
    path = f'{DEST}/{name}'
    mat = unreal.load_asset(path)
    if mat: LIB.delete_all_material_expressions(mat)
    else: mat = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    build(mat)
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    unreal.log(f'CHUCK_FEY_MATERIAL {name}')
    return mat


# ---------------------------------------------------------------- music and textures
if not ONLY:
    task = unreal.AssetImportTask(); task.filename = str(ROOT / 'SourceAssets/Audio/Feywild_main.wav'); task.destination_path = '/Game/Art/Audio'
    task.destination_name = 'SW_FeywildMain'; task.automated = True; task.replace_existing = True; task.save = True
    TOOLS.import_asset_tasks([task]); wave = unreal.load_asset('/Game/Art/Audio/SW_FeywildMain')
    if not isinstance(wave, unreal.SoundWave): raise RuntimeError('SW_FeywildMain import failed')
    wave.set_editor_property('looping', True); ASSETS.save_loaded_asset(wave, only_if_is_dirty=False)
    unreal.log(f'CHUCK_AUDIO_IMPORTED SW_FeywildMain duration={wave.get_editor_property("duration"):.2f}')

if not ONLY or os.environ.get('CHUCK_FEY_SOUND') == '1':
    task = unreal.AssetImportTask(); task.filename = str(ROOT / 'SourceAssets/Audio/SFX/SFX_FeyFalls_00.wav'); task.destination_path = '/Game/Art/Audio/SFX'
    task.destination_name = 'SFX_FeyFalls_00'; task.automated = True; task.replace_existing = True; task.save = True
    TOOLS.import_asset_tasks([task]); wave = unreal.load_asset('/Game/Art/Audio/SFX/SFX_FeyFalls_00')
    if not isinstance(wave, unreal.SoundWave): raise RuntimeError('SFX_FeyFalls_00 import failed')
    wave.set_editor_property('looping', True); ASSETS.save_loaded_asset(wave, only_if_is_dirty=False)

textures = {}
for rec in json.loads((SOURCE / 'manifest.json').read_text(encoding='utf-8-sig'))['assets']:
    if rec['role'] == 'blend': continue
    file = SOURCE / rec['file']; name = 'T_' + file.stem; role = rec['role'].lower()
    if not ONLY:
        if hashlib.sha256(file.read_bytes()).hexdigest().upper() != rec['sha256']: raise RuntimeError(file.name + ' hash')
        task = unreal.AssetImportTask(); task.filename = str(file); task.destination_path = DEST; task.destination_name = name
        task.automated = True; task.replace_existing = True; task.save = False
        TOOLS.import_asset_tasks([task]); tex = unreal.load_asset(f'{DEST}/{name}')
        tex.set_editor_property('srgb', role == 'diffuse')
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP if role == 'nor_dx' else
                                unreal.TextureCompressionSettings.TC_DEFAULT if role == 'diffuse' else unreal.TextureCompressionSettings.TC_MASKS)
        ASSETS.save_loaded_asset(tex, only_if_is_dirty=False)
    textures[rec['asset'], role] = unreal.load_asset(f'{DEST}/{name}')


SAMPLER = {'diffuse': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, 'nor_dx': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
           'arm': unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 'alpha': unreal.MaterialSamplerType.SAMPLERTYPE_MASKS}


def tobj(m, tex, role): return node(m, unreal.MaterialExpressionTextureObject, texture=tex, sampler_type=SAMPLER[role])
def fey(m, asset, role): return tobj(m, textures[asset, role], role)
def chult(m, name, role): return tobj(m, unreal.load_asset(f'{NATURE}/{name}'), role)


NOISE = r'''
struct Fn
{
    float H(float2 i){float3 p=frac(float3(i.xyx)*.1031);p+=dot(p,p.yzx+33.33);return frac((p.x+p.y)*p.z);}
    float N(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(H(i),H(i+float2(1,0)),f.x),lerp(H(i+float2(0,1)),H(i+1),f.x),f.y);}
    float B(float2 p){return N(p)*.5+N(p*2.03+7.1)*.3+N(p*4.01+3.3)*.2;}
    float3 Fade(float h){h=frac(h)*4.;float i=floor(h),k=smoothstep(.15,.85,h-i);
     float3 a=i<.5?float3(.08,.8,.66):i<1.5?float3(.16,.42,1.):i<2.5?float3(1.,.68,.2):float3(1.,.36,.34);
     float3 b=i<.5?float3(.16,.42,1.):i<1.5?float3(1.,.68,.2):i<2.5?float3(1.,.36,.34):float3(.08,.8,.66);
     return lerp(a,b,k);}
};
Fn F;
'''

# ---------------------------------------------------------------- the sky: blue, warming to peach and gold, never purple
SKY = NOISE + r'''
float3 d=normalize(Dir);float3 s=normalize(Sun);
float h=saturate(d.z);
float az=saturate(dot(normalize(d.xy+1e-4),normalize(s.xy))*.5+.5);
float3 zen=float3(.045,.15,.46),mid=float3(.2,.43,.78);
float3 hor=lerp(lerp(float3(.42,.66,.9),float3(.96,.84,.66),smoothstep(.62,.88,az)),float3(1.1,.62,.3),smoothstep(.86,.995,az));
float3 c=lerp(hor,mid,smoothstep(.0,.09+.09*az,h));c=lerp(c,zen,smoothstep(.16,.8,h));
// High streaks of cloud, peach toward the sun, pale blue-white away from it.
float2 q=d.xy/(d.z+.16);
float wisp=F.B(q*float2(1.6,3.4)+float2(T*.006,0));wisp=smoothstep(.47,.74,wisp+.1*F.B(q*9.));
float low=smoothstep(.0,.1,h)*(1-smoothstep(.55,.95,h));
float3 cl=lerp(lerp(float3(.66,.82,1.),float3(1.05,.95,.82),smoothstep(.6,.86,az)),float3(1.5,.86,.44),smoothstep(.84,.99,az));
c=lerp(c,cl,wisp*low*.82);
float sd=saturate(dot(d,s));
c+=float3(1.6,.86,.36)*pow(sd,26.)*.42+float3(3.,1.8,.8)*pow(sd,120.)*1.1+float3(30.,20.,10.)*pow(sd,2600.);
// Under the horizon, the far haze.
c=lerp(lerp(float3(.34,.46,.56),float3(1.,.68,.42),pow(az,3.)),c,smoothstep(-.03,.012,d.z));
return c*.42;
'''


def sky(m):
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT); m.set_editor_property('two_sided', True); m.set_editor_property('is_sky', True)
    sun = node(m, unreal.MaterialExpressionVectorParameter, parameter_name='SunDir', default_value=unreal.LinearColor(.985, .14, .1, 0))
    out(custom(m, SKY, {'Dir': node(m, unreal.MaterialExpressionLocalPosition), 'T': node(m, unreal.MaterialExpressionTime), 'Sun': sun}), P.MP_EMISSIVE_COLOR)


# ---------------------------------------------------------------- the ground. Vertex colour R the path, G moss, B damp
GROUND = NOISE + r'''
float2 p=P.xy;
float mac=F.B(p/640.);
float3 soil=Texture2DSample(SD,SDSampler,p/300.).rgb*float3(.62,.56,.46);
float3 mud=Texture2DSample(MD,MDSampler,p/240.).rgb*float3(1.02,.82,.66);
float sp=F.N(p/6.)*.3+F.N(p/2.1)*.36+F.N(p/.7)*.34;
float3 moss=lerp(float3(.032,.085,.018),float3(.13,.2,.03),smoothstep(.26,.74,sp))*lerp(.78,1.2,mac);
float mk=saturate(VC.g*1.5+(mac-.5)*1.1);mk=smoothstep(.2,.7,mk+(sp-.5)*.18);
float pk=smoothstep(.35,.68,VC.r+(F.B(p/46.)-.5)*.5);
float3 c=lerp(lerp(soil,moss,mk),mud,pk);
float st=smoothstep(.82,.55,normalize(N).z+(F.B(p/90.)-.5)*.16);
float2 ru=float2(P.x*.6+P.y,P.z)/460.;
c=lerp(c,Texture2DSample(RD,RDSampler,ru).rgb*float3(.66,.68,.64),st);
float3 ta=Texture2DSample(SA,SASampler,p/300.).rgb,tb=Texture2DSample(MA,MASampler,p/240.).rgb;
float rg=lerp(lerp(ta.g,.85,mk),tb.g,pk),ao=lerp(lerp(ta.r,1.,mk),tb.r,pk);
float2 ns=Texture2DSample(SN,SNSampler,p/300.).rg*2-1,nm=Texture2DSample(MN,MNSampler,p/240.).rg*2-1;
float2 tn=lerp(lerp(ns,float2(F.N(p/2.3)-.5,F.N(p/2.3+9.)-.5)*1.1,mk),nm,pk);
tn=lerp(tn,Texture2DSample(RN,RNSampler,ru).rg*2-1,st);rg=lerp(rg,.82,st);
// Damp near water; rain-held puddles in the path's hollows hold the sky.
float pud=smoothstep(.77,.8,F.B(p/90.+4.))*saturate(VC.r*1.4)*saturate(VC.b*1.6+.35);
float wet=saturate(VC.b*.8);
c*=lerp(1.,.58,wet);rg=lerp(rg,.52,wet);
c=lerp(c,float3(.012,.018,.026),pud);rg=lerp(rg,.1,pud);tn*=1-pud*.8;
// Vertex alpha: how much light reaches it (1 in the open; low inside the river's root tunnel, 2026-11-19).
c*=lerp(.03,1.,VA);
Rough=rg;AO=ao*lerp(.25,1.,VA);
WN=normalize(normalize(N)+float3(tn*1.2,0));
return c;
'''


def ground(m):
    m.set_editor_property('tangent_space_normal', False)
    vc = node(m, unreal.MaterialExpressionVertexColor)
    inputs = {'P': node(m, unreal.MaterialExpressionLocalPosition), 'N': node(m, unreal.MaterialExpressionVertexNormalWS), 'VC': vc, 'VA': (vc, 'A'),
              'SD': chult(m, 'T_forest_ground_04_diff_2k', 'diffuse'), 'SN': chult(m, 'T_forest_ground_04_nor_dx_2k', 'nor_dx'), 'SA': chult(m, 'T_forest_ground_04_arm_2k', 'arm'),
              'MD': fey(m, 'brown_mud_leaves_01', 'diffuse'), 'MN': fey(m, 'brown_mud_leaves_01', 'nor_dx'), 'MA': fey(m, 'brown_mud_leaves_01', 'arm'),
              'RD': tobj(m, unreal.load_asset('/Game/Art/ChultGorge/T_cliff_side_diff_2k'), 'diffuse'), 'RN': tobj(m, unreal.load_asset('/Game/Art/ChultGorge/T_cliff_side_nor_dx_2k'), 'nor_dx')}
    c = custom(m, GROUND, inputs, extra=[('Rough', F1), ('AO', F1), ('WN', F3)])
    out(c, P.MP_BASE_COLOR); out(c, P.MP_ROUGHNESS, 'Rough'); out(c, P.MP_AMBIENT_OCCLUSION, 'AO'); out(c, P.MP_NORMAL, 'WN')
    out(const(m, .22), P.MP_SPECULAR)


# ---------------------------------------------------------------- the oaks' otherworldly light (the user, 2026-11-18)
# Fn.Fade is a slow fade through teal, blue, gold and rose, about a minute a turn, each part of a tree at its own point in the turn.

# ---------------------------------------------------------------- bark, mossed by vertex colour R and on what faces up
BARK = NOISE + r'''
float3 n=normalize(LN);float3 w=pow(abs(n),5.);w/=dot(w,1);float S=170.;
float3 c=Texture2DSample(D,DSampler,P.yz/S).rgb*w.x+Texture2DSample(D,DSampler,P.xz/S).rgb*w.y+Texture2DSample(D,DSampler,P.xy/S).rgb*w.z;
float3 a=Texture2DSample(A,ASampler,P.yz/S).rgb*w.x+Texture2DSample(A,ASampler,P.xz/S).rgb*w.y+Texture2DSample(A,ASampler,P.xy/S).rgb*w.z;
float2 nx=Texture2DSample(NM,NMSampler,P.yz/S).rg*2-1,ny=Texture2DSample(NM,NMSampler,P.xz/S).rg*2-1,nz=Texture2DSample(NM,NMSampler,P.xy/S).rg*2-1;
float3 dn=float3(0,nx.x,nx.y)*w.x+float3(ny.x,0,ny.y)*w.y+float3(nz.x,nz.y,0)*w.z;
float sp=0,big=0;float3 pl[3]={float3(P.yz,w.x),float3(P.xz,w.y),float3(P.xy,w.z)};
for(int i=0;i<3;i++){float2 uv=pl[i].xy;sp+=(F.N(uv/9.)*.45+F.N(uv/3.)*.33+F.N(uv/1.1)*.22)*pl[i].z;big+=F.B(uv/120.)*pl[i].z;}
float mk=saturate(VC.r*.95+saturate(n.z)*.75+(big-.5)*1.3-.32);mk=smoothstep(.25,.6,mk+(sp-.5)*.45+(c.g-.25)*.5);
float3 moss=lerp(float3(.018,.055,.012),float3(.15,.23,.035),smoothstep(.25,.8,sp));
Base=lerp(c*float3(.42,.34,.3),moss,mk);
Rough=lerp(a.g,.9,mk);AO=a.r;
// Thin veins of light up through parts of the bark, only in some patches, pulsing slowly upward and changing colour.
float vn=0;for(int i=0;i<3;i++){float2 uv=pl[i].xy/float2(34.,120.);uv+=(float2(F.N(uv*1.7+4.),F.N(uv*1.7+9.))-.5)*.45;
 float r1=1.-abs(F.N(uv)*2.-1.),r2=1.-abs(F.N(uv*2.7+5.)*2.-1.);vn+=max(smoothstep(.958,.998,r1),.5*smoothstep(.97,.998,r2))*pl[i].z;}
float zone=smoothstep(.55,.72,F.B(float2(P.x+P.y,P.z)/430.+Rnd*17.+T*.006));
float pulse=.5+.5*sin(T*.3-P.z*.011+Rnd*6.28);
Glow=F.Fade(T/62.+big*1.6+P.z/1500.+Rnd)*vn*zone*(1.-mk*.9)*lerp(.25,1.,pulse)*.5;
return normalize(n+dn*lerp(1.1,.25,mk));
'''


# The river's root tunnel (2026-11-19): the same bark, its light from vertex colour G (dark inside, open at the mouth).
ROOT = BARK.replace('return normalize(n+dn', 'Base*=lerp(.03,1.,VC.g);AO*=lerp(.25,1.,VC.g);Glow*=lerp(.2,.4,VC.g);\nreturn normalize(n+dn')


def bark(m, code=BARK):
    m.set_editor_property('tangent_space_normal', False)
    ln = node(m, unreal.MaterialExpressionTransform, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD, transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    link(node(m, unreal.MaterialExpressionVertexNormalWS), ln, '')
    c = custom(m, code, {'P': node(m, unreal.MaterialExpressionLocalPosition), 'LN': ln, 'VC': node(m, unreal.MaterialExpressionVertexColor), 'D': chult(m, 'T_tree_bark_03_diff_2k', 'diffuse'),
                         'NM': chult(m, 'T_tree_bark_03_nor_dx_2k', 'nor_dx'), 'A': chult(m, 'T_tree_bark_03_arm_2k', 'arm'), 'T': node(m, unreal.MaterialExpressionTime), 'Rnd': node(m, unreal.MaterialExpressionPerInstanceRandom)}, extra=[('Base', F3), ('Rough', F1), ('AO', F1), ('Glow', F3)])
    wn = node(m, unreal.MaterialExpressionTransform, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL, transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    link(c, wn, '')
    out(wn, P.MP_NORMAL); out(c, P.MP_BASE_COLOR, 'Base'); out(c, P.MP_ROUGHNESS, 'Rough'); out(c, P.MP_AMBIENT_OCCLUSION, 'AO'); out(c, P.MP_EMISSIVE_COLOR, 'Glow'); out(const(m, .3), P.MP_SPECULAR)


# ---------------------------------------------------------------- plants and fungi: vertex colour RGB, glow in A
def plant(m):
    m.set_editor_property('two_sided', True); m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    vc = node(m, unreal.MaterialExpressionVertexColor)
    c = custom(m, 'float3 L=pow(max(VC,.0001),1.9);Base=L;Sub=L*.65;return L*VA*VA*10.*Gain;', {'VC': vc, 'VA': (vc, 'A'), 'Gain': const(m, 2.2)}, extra=[('Base', F3), ('Sub', F3)])
    out(c, P.MP_BASE_COLOR, 'Base'); out(c, P.MP_EMISSIVE_COLOR); out(c, P.MP_SUBSURFACE_COLOR, 'Sub'); out(const(m, .62), P.MP_ROUGHNESS); out(const(m, .25), P.MP_SPECULAR)


# Fungi and bell flowers. UV.x runs round the part, UV.y says which part: 0 gills, 1 dome, 2 stem, 3 bell, 4 the bell's lit heart.
FUNGUS = NOISE + r'''
float3 L=pow(max(VC,.0001),1.9);
float vv=1.-UV.y,part=floor(vv),t=saturate(frac(vv)/.98),u=UV.x;   // (the FBX import flips V)
float3 c=L,tn=float3(0,0,1);float rg=.55,em=1.;
if(part<.5){
 float g=frac(u*Count),tri=abs(g-.5)*2.,fine=abs(frac(u*Count*3.)-.5)*2.;
 float ridge=smoothstep(.12,.8,lerp(tri,fine,.3*smoothstep(.35,.8,t)));
 c*=lerp(.26,1.15,ridge);
 em=lerp(.1,1.,ridge)*lerp(1.9,.45,smoothstep(0.,.9,t))*(.8+.4*F.N(float2(u*Count*.7,t*5.)));
 tn.x=(g<.5?1.:-1.)*.55;rg=.62;
}else if(part<1.5){
 float st=F.B(float2(u*38.,t*2.2)),mo=F.B(float2(u*9.,t*5.));
 float2 q=float2(u*26.,t*11.),id=floor(q),f=frac(q)-.5;float h=F.H(id);
 float2 o=float2(F.H(id+3.1),F.H(id+7.7))-.5;
 float sp=smoothstep(.1+.16*h,.03,length(f-o*.55))*step(.48,h)*smoothstep(.97,.7,t);
 c*=lerp(.62,1.25,st)*lerp(.8,1.15,mo);c=lerp(c,c*1.6+.035,sp);
 rg=lerp(.3,.52,mo);em=.35*smoothstep(.35,0.,t)+.15*st+sp*3.2;
 float e=.015;tn.x=(F.B(float2((u+e)*38.,t*2.2))-F.B(float2((u-e)*38.,t*2.2)))*.8;tn.y=(mo-.5)*.22;
}else if(part<2.5){
 float fb=F.B(float2(u*16.,t*1.6)),sc=F.N(float2(u*9.,t*26.));
 c*=lerp(.6,1.2,fb)*lerp(.85,1.05,sc)*lerp(.5,1.,smoothstep(0.,.2,t));
 rg=.66;em=lerp(.35,1.3,t*t)*(.6+.6*fb);tn.x=(fb-.5)*1.2;tn.y=(sc-.5)*.5;
}else if(part<3.5){
 float rib=abs(cos(u*31.4159)),vn=abs(frac(u*44.)-.5)*2.;
 c*=lerp(.72,1.1,rib)*lerp(.85,1.1,smoothstep(.2,.9,vn));
 rg=.42;em=(.3+.9*pow(t,1.6))*lerp(.7,1.1,smoothstep(.1,.7,vn))*lerp(.7,1.15,rib);tn.x=-sin(u*31.4159)*sign(cos(u*31.4159))*.35;
}
Base=c;Sub=c*.7;Rough=rg;TN=normalize(tn);
return L*VA*VA*10.*Gain*em;
'''


def fungus(m, count):
    m.set_editor_property('two_sided', True); m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    vc = node(m, unreal.MaterialExpressionVertexColor)
    c = custom(m, FUNGUS, {'VC': vc, 'VA': (vc, 'A'), 'UV': node(m, unreal.MaterialExpressionTextureCoordinate), 'Count': const(m, count), 'Gain': const(m, 2.2)},
               extra=[('Base', F3), ('Sub', F3), ('Rough', F1), ('TN', F3)])
    out(c, P.MP_BASE_COLOR, 'Base'); out(c, P.MP_EMISSIVE_COLOR); out(c, P.MP_SUBSURFACE_COLOR, 'Sub'); out(c, P.MP_ROUGHNESS, 'Rough'); out(c, P.MP_NORMAL, 'TN'); out(const(m, .38), P.MP_SPECULAR)


# Leaves. UV.x along the blade, UV.y across it.
BLADE = NOISE + r'''
float3 L=pow(max(VC,.0001),1.9);
float u=UV.x,v=UV.y*2.-1.,av=abs(v);
float mid=smoothstep(.05,.008,av)*smoothstep(1.,.75,u);
float s=frac(u*Veins-av*1.9),sd=abs(s-.5)*2.;
float side=smoothstep(.93,.99,sd)*smoothstep(1.,.82,av);
float mott=F.B(float2(u*11.,v*6.+3.)),fine=F.N(float2(u*70.,v*38.));
float3 c=L*lerp(.58,1.25,mott)*lerp(.86,1.1,fine);
c=lerp(c,Vein,saturate(mid*.6+side*.3));
c*=lerp(1.,.6,smoothstep(.72,1.,av));
c=lerp(c,c*float3(1.25,.85,.42),smoothstep(.84,1.,u)*.65);
float2 q=float2(u*22.,v*8.),id=floor(q),f=frac(q)-.5;float h=F.H(id);
float dew=smoothstep(.2,.1,length(f-(float2(F.H(id+2.3),F.H(id+5.9))-.5)*.5))*step(.72,h);
Rough=lerp(lerp(.34,.58,mott),.05,dew);
float3 tn=float3(cos(s*6.2832)*.3*(1-mid),sign(v)*-.45*mid+sin(s*6.2832)*.12,1);
tn.xy+=(float2(F.N(float2(u*40.,v*20.)),F.N(float2(u*40.,v*20.)+9.))-.5)*.22;
TN=normalize(tn);Base=c;Sub=c*float3(1.15,1.25,.7)*.8;
return L*VA*VA*10.*2.2;
'''

PETAL = NOISE + r'''
float3 L=pow(max(VC,.0001),1.9);
float u=UV.x,v=UV.y*2.-1.;
float st=F.B(float2(u*3.,v*13.)),fn=F.N(float2(u*30.,v*30.));
float3 c=L*lerp(.7,1.22,st)*lerp(.92,1.06,fn);
c=lerp(c,c*float3(1.25,1.1,1.1)+.03,smoothstep(.15,0.,u));
Rough=lerp(.36,.56,st);TN=normalize(float3((st-.5)*.25,(F.B(float2(u*3.,v*13.+.2))-st)*3.,1));Base=c;Sub=c*.9;
return L*VA*VA*10.*2.2;
'''

PAD = NOISE + r'''
float3 L=pow(max(VC,.0001),1.9);
float r=length(P.xy)/50.,a=atan2(P.y,P.x);
float sp=abs(frac(a*22./6.2832)-.5)*2.,fork=abs(frac(a*44./6.2832)-.5)*2.;
float vein=max(smoothstep(.82,.98,sp)*smoothstep(.02,.15,r),smoothstep(.86,.98,fork)*smoothstep(.45,.6,r)*.7);
float mott=F.B(P.xy/9.),fine=F.N(P.xy/1.3);
float3 c=L*lerp(.6,1.32,mott)*lerp(.9,1.08,fine);
c=lerp(c,float3(.2,.31,.08),vein*.5);
c=lerp(c,float3(.16,.035,.035),smoothstep(.94,.995,r));
float2 q=P.xy/2.1,id=floor(q),f=frac(q)-.5;float h=F.H(id);
float dew=smoothstep(.24,.12,length(f-(float2(F.H(id+2.3),F.H(id+5.9))-.5)*.5))*step(.86,h);
Rough=lerp(lerp(.38,.56,mott),.05,dew);
float2 ou=normalize(P.xy+1e-3),ac=float2(-ou.y,ou.x);
float3 n=normalize(float3(ou*sin(r*34.)*.05+ac*(frac(a*22./6.2832)-.5)*.5*smoothstep(.05,.3,r),1));
TN=n;Base=c;
return 0;
'''


def blade_mat(m, vein, veins):
    m.set_editor_property('two_sided', True); m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    vc = node(m, unreal.MaterialExpressionVertexColor)
    c = custom(m, BLADE, {'VC': vc, 'VA': (vc, 'A'), 'UV': node(m, unreal.MaterialExpressionTextureCoordinate), 'Veins': const(m, veins), 'Vein': node(m, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(*vein, 1))},
               extra=[('Base', F3), ('Sub', F3), ('Rough', F1), ('TN', F3)])
    out(c, P.MP_BASE_COLOR, 'Base'); out(c, P.MP_EMISSIVE_COLOR); out(c, P.MP_SUBSURFACE_COLOR, 'Sub'); out(c, P.MP_ROUGHNESS, 'Rough'); out(c, P.MP_NORMAL, 'TN'); out(const(m, .42), P.MP_SPECULAR)


def petal_mat(m):
    m.set_editor_property('two_sided', True); m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    vc = node(m, unreal.MaterialExpressionVertexColor)
    c = custom(m, PETAL, {'VC': vc, 'VA': (vc, 'A'), 'UV': node(m, unreal.MaterialExpressionTextureCoordinate)}, extra=[('Base', F3), ('Sub', F3), ('Rough', F1), ('TN', F3)])
    out(c, P.MP_BASE_COLOR, 'Base'); out(c, P.MP_EMISSIVE_COLOR); out(c, P.MP_SUBSURFACE_COLOR, 'Sub'); out(c, P.MP_ROUGHNESS, 'Rough'); out(c, P.MP_NORMAL, 'TN'); out(const(m, .35), P.MP_SPECULAR)


def pad_mat(m):
    m.set_editor_property('two_sided', True); m.set_editor_property('tangent_space_normal', False)
    c = custom(m, PAD, {'VC': node(m, unreal.MaterialExpressionVertexColor), 'P': node(m, unreal.MaterialExpressionLocalPosition)}, extra=[('Base', F3), ('Rough', F1), ('TN', F3)])
    out(c, P.MP_BASE_COLOR, 'Base'); out(c, P.MP_ROUGHNESS, 'Rough'); out(c, P.MP_NORMAL, 'TN'); out(const(m, .24), P.MP_SPECULAR)


# The far land and its trees, unlit: their colour is the distance they stand at.
FAR = NOISE + r'''
float d=pow(saturate((W.x-O)/520000.),.38);
float3 c=lerp(float3(.012,.04,.035),float3(.03,.085,.1),smoothstep(.2,.5,d));
c=lerp(c,float3(.09,.2,.31),smoothstep(.5,.78,d));
c=lerp(c,float3(.19,.31,.45),smoothstep(.78,1.,d));
c*=lerp(.72,1.25,F.B(W.xy/(900.+d*9000.)));
c*=lerp(.68,1.3,F.B(W.xy/170.+W.z/210.));
if(Rnd>.9)c=lerp(c,float3(.2,.08,.17),.3*(1-d));else if(Rnd>.8)c=lerp(c,float3(.2,.14,.04),.3*(1-d));
c*=lerp(.55,1.2,VC);
float3 s=normalize(Sun);
float rim=pow(saturate(dot(normalize(N),s)*.5+.5),4.);
float az=pow(saturate(dot(normalize(-Cam),s)),10.);
return c*lerp(1.,1.7,az)+float3(1.1,.62,.28)*rim*lerp(.16,.14,d)+float3(.9,.5,.22)*az*d*.35;
'''


def far(m):
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    sun = node(m, unreal.MaterialExpressionVectorParameter, parameter_name='SunDir', default_value=unreal.LinearColor(.96, -.2, .17, 0))
    orig = node(m, unreal.MaterialExpressionScalarParameter, parameter_name='OriginX', default_value=0.)
    out(custom(m, FAR, {'W': node(m, unreal.MaterialExpressionWorldPosition), 'O': orig, 'VC': (node(m, unreal.MaterialExpressionVertexColor), 'R'), 'N': node(m, unreal.MaterialExpressionVertexNormalWS),
                        'Cam': node(m, unreal.MaterialExpressionCameraVectorWS), 'Sun': sun, 'Rnd': node(m, unreal.MaterialExpressionPerInstanceRandom)}), P.MP_EMISSIVE_COLOR)


RIPPLE = r'''
float r=length(UV-.5)*2.;
float a=saturate(Age),R0=.14+.78*sqrt(a),R1=R0*.62,w=.035+.07*a;
float i=smoothstep(w,0.,abs(r-R0))+.55*smoothstep(w,0.,abs(r-R1));
Op=saturate(i*(1-a)*(1-a)*A)*smoothstep(1.,.9,r);
return float3(.74,.84,.92);
'''


def ripple(m):
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT); m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT); m.set_editor_property('two_sided', True)
    c = custom(m, RIPPLE, {'UV': node(m, unreal.MaterialExpressionTextureCoordinate), 'Age': node(m, unreal.MaterialExpressionPerInstanceCustomData, data_index=0), 'A': node(m, unreal.MaterialExpressionPerInstanceCustomData, data_index=1)}, extra=[('Op', F1)])
    out(c, P.MP_EMISSIVE_COLOR); out(c, P.MP_OPACITY, 'Op')


WIND = r'''
float k=sin(T*1.25+W.x*.013+W.y*.017)+.5*sin(T*2.9+W.x*.041-W.y*.03);
return float3(k,k*.6,0)*Amp*saturate(H);
'''


LEAF_LIGHT = NOISE + r'''
// Soft drifting patches of the crown take the light along the leaves' veins, fading through the same colours as the bark.
float zone=smoothstep(.54,.74,F.B(float2(P.x+P.z*.6,P.y-P.z*.4)/300.+Rnd*13.+T*.008));
float l=dot(D,float3(.3,.6,.1));
return F.Fade(T/62.+P.z/1300.+Rnd+.35)*zone*saturate(l*3.)*l*(.6+.4*sin(T*.24+P.x*.006+Rnd*6.28))*1.5;
'''


def foliage(m, diff, nrm, arm, alpha, tint, sub, amp, height, clip=.4, light=False):
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED); m.set_editor_property('two_sided', True)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE); m.set_editor_property('opacity_mask_clip_value', clip)
    tex = lambda t, typ: node(m, unreal.MaterialExpressionTextureSample, texture=t, sampler_type=typ)
    vc = node(m, unreal.MaterialExpressionVertexColor); d = tex(diff, SAMPLER['diffuse'])
    c = custom(m, 'Sub=D*Tint*float3(.62,.56,.26);return D*Tint*lerp(1.,Shade,Use);', {'D': d, 'Tint': node(m, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(*tint, 1)),
               'Shade': (vc, 'R'), 'Use': const(m, 1. if height == 0 else 0.)}, extra=[('Sub', F3)])
    out(c, P.MP_BASE_COLOR); out(c, P.MP_SUBSURFACE_COLOR, 'Sub'); out(tex(nrm, SAMPLER['nor_dx']), P.MP_NORMAL)
    if light:
        out(custom(m, LEAF_LIGHT, {'D': d, 'UV': node(m, unreal.MaterialExpressionTextureCoordinate), 'P': node(m, unreal.MaterialExpressionLocalPosition), 'T': node(m, unreal.MaterialExpressionTime), 'Rnd': node(m, unreal.MaterialExpressionPerInstanceRandom)}), P.MP_EMISSIVE_COLOR)
    a = tex(arm, SAMPLER['arm']); out(a, P.MP_ROUGHNESS, 'G'); out(a, P.MP_AMBIENT_OCCLUSION, 'R'); out(tex(alpha, SAMPLER['alpha']), P.MP_OPACITY_MASK, 'R'); out(const(m, .35), P.MP_SPECULAR)
    h = custom(m, 'return P.z/Hh;' if height else 'return 1;', {'P': node(m, unreal.MaterialExpressionLocalPosition), 'Hh': const(m, height or 1.)}, F1)
    out(custom(m, WIND, {'T': node(m, unreal.MaterialExpressionTime), 'W': node(m, unreal.MaterialExpressionWorldPosition), 'Amp': const(m, amp), 'H': h}), P.MP_WORLD_POSITION_OFFSET)


def nat(name): return unreal.load_asset(f'{NATURE}/{name}')


# ---------------------------------------------------------------- water
WATER = NOISE + r'''
float2 p=P.xy;
float2 g=0;float e=3.;
float2 a=p/95.+float2(T*.05,T*.03),b=p/37.+float2(-T*.07,T*.05);
g.x=(F.B(a+float2(e/95.,0))-F.B(a-float2(e/95.,0)))*.55+(F.N(b+float2(e/37.,0))-F.N(b-float2(e/37.,0)))*.3;
g.y=(F.B(a+float2(0,e/95.))-F.B(a-float2(0,e/95.)))*.55+(F.N(b+float2(0,e/37.))-F.N(b-float2(0,e/37.)))*.3;
float churn=saturate(VC.g);
Foam=smoothstep(.42,.7,F.B(p/26.+float2(0,-T*.32))+churn*.25)*churn;
Rough=lerp(.3,.5,Foam);
float3 n=normalize(float3(-g*(.22+churn*3.),1));
Base=lerp(lerp(float3(.004,.016,.02),float3(.022,.05,.04),saturate(VC.r)),float3(.72,.84,.9),Foam);
// The pond mirrors a capture of the glade taken from its middle (AFeywildSetting::BuildProbe): the reflected ray is carried
// out to an ellipsoid about as far off as the trees and falls, so what it finds lines up from wherever he stands.
float3 v=normalize(Cam),r=reflect(-v,n);r.z=abs(r.z)+.012;r=normalize(r);
float3 o=P-Probe,sc=float3(1./2100.,1./2100.,1./1500.),ao=o*sc,bo=r*sc;
float A=dot(bo,bo),B=dot(ao,bo),CC=dot(ao,ao)-1.;
float3 dir=normalize(o+r*((-B+sqrt(max(B*B-A*CC,0.)))/A));
float3 m=min(TextureCubeSampleLevel(Cube,CubeSampler,dir,0.).rgb,float3(7.,7.,7.));
float fr=saturate(.34+.66*pow(1.-saturate(dot(n,v)),3.));
Mirror=m*fr*(1.-Foam);
return n;
'''


def water(m):
    m.set_editor_property('tangent_space_normal', False)
    cube = node(m, unreal.MaterialExpressionTextureObjectParameter, parameter_name='Reflection', texture=unreal.load_asset('/Engine/EngineResources/DefaultTextureCube'), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    probe = node(m, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(3400., 1300., 60., 1))
    c = custom(m, WATER, {'P': node(m, unreal.MaterialExpressionLocalPosition), 'T': node(m, unreal.MaterialExpressionTime), 'VC': node(m, unreal.MaterialExpressionVertexColor), 'Cam': node(m, unreal.MaterialExpressionCameraVectorWS),
                          'Cube': cube, 'Probe': probe}, extra=[('Base', F3), ('Rough', F1), ('Foam', F1), ('Mirror', F3)])
    out(c, P.MP_NORMAL); out(c, P.MP_BASE_COLOR, 'Base'); out(c, P.MP_ROUGHNESS, 'Rough'); out(c, P.MP_EMISSIVE_COLOR, 'Mirror'); out(const(m, 0.), P.MP_SPECULAR)


# The running water (2026-11-19): the cutscene's river (AFeywildSetting::BuildRiver) and the slice's streams above the falls.
# A ribbon whose UV is (along, across)/200 cm, the current running along U at a speed set by its churn. Vertex colour: R
# toward the banks, G churn, B light (low in the root tunnel). It reflects what is on screen (SSR, specular on), with only a
# trace of the glade's capture where that finds nothing.
RIVER = NOISE + r'''
float2 q=UV*200.;
float churn=saturate(VC.g),sp=lerp(170.,290.,churn),lit=saturate(VC.b);
float2 a=float2(q.x-T*sp,q.y)/float2(150.,70.),b=float2(q.x-T*sp*1.3,q.y)/float2(48.,22.);
float e=.03;float2 g;
g.x=(F.B(a+float2(e,0))-F.B(a-float2(e,0)))*.55+(F.N(b+float2(e,0))-F.N(b-float2(e,0)))*.35;
g.y=(F.B(a+float2(0,e))-F.B(a-float2(0,e)))*.55+(F.N(b+float2(0,e))-F.N(b-float2(0,e)))*.35;
float streak=F.B(float2(q.x-T*sp,q.y*1.8)/float2(70.,20.));
Foam=smoothstep(.66,.86,streak+churn*.2)*churn*(.35+.65*saturate(VC.r));
Rough=lerp(lerp(.08,.22,churn),.55,Foam);
Spec=.5*lit;
float3 n=normalize(float3(-g*(.6+churn*1.5),1));
Base=lerp(lerp(float3(.004,.014,.016),float3(.02,.045,.034),saturate(VC.r)),float3(.4,.5,.55),Foam)*lerp(.04,1.,lit);
float3 v=normalize(Cam),r=reflect(-v,n);r.z=abs(r.z)+.02;r=normalize(r);
float3 m=min(TextureCubeSampleLevel(Cube,CubeSampler,r,0.).rgb,float3(4.,4.,4.));
float fr=saturate(.25+.75*pow(1.-saturate(dot(n,v)),4.));
Mirror=m*fr*(1.-Foam)*max(lit,.08)*.12;
return n;
'''


def river(m):
    m.set_editor_property('tangent_space_normal', False)
    cube = node(m, unreal.MaterialExpressionTextureObjectParameter, parameter_name='Reflection', texture=unreal.load_asset('/Engine/EngineResources/DefaultTextureCube'), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    c = custom(m, RIVER, {'UV': node(m, unreal.MaterialExpressionTextureCoordinate), 'T': node(m, unreal.MaterialExpressionTime), 'VC': node(m, unreal.MaterialExpressionVertexColor), 'Cam': node(m, unreal.MaterialExpressionCameraVectorWS),
                          'Cube': cube}, extra=[('Base', F3), ('Rough', F1), ('Foam', F1), ('Mirror', F3), ('Spec', F1)])
    out(c, P.MP_NORMAL); out(c, P.MP_BASE_COLOR, 'Base'); out(c, P.MP_ROUGHNESS, 'Rough'); out(c, P.MP_EMISSIVE_COLOR, 'Mirror'); out(c, P.MP_SPECULAR, 'Spec')


FALL = NOISE + r'''
float2 u=UV*float2(16.,3.);
float s=F.B(float2(u.x,u.y-T*1.5))*.6+F.B(float2(u.x*2.3+4.,u.y*1.5-T*2.3))*.4;
Op=saturate(smoothstep(.3,.68,s)*VA*(.55+.6*UV.y));
return lerp(float3(.26,.4,.5),float3(.85,.92,.95),smoothstep(.45,.8,s))*(.7+.5*VC.r);
'''


def fall(m):
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT); m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT); m.set_editor_property('two_sided', True)
    vc = node(m, unreal.MaterialExpressionVertexColor)
    c = custom(m, FALL, {'UV': node(m, unreal.MaterialExpressionTextureCoordinate), 'T': node(m, unreal.MaterialExpressionTime), 'VC': vc, 'VA': (vc, 'A')}, extra=[('Op', F1)])
    out(c, P.MP_EMISSIVE_COLOR); out(c, P.MP_OPACITY, 'Op')


# ---------------------------------------------------------------- lights in the air. Custom data 0 brightness, 1 hue (0 gold, 1 blue)
def mote(m):
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT); m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE); m.set_editor_property('two_sided', True)
    code = 'float r=saturate(1-length(UV-.5)*2.);float g=r*r*r+pow(r,14.)*5.;return lerp(lerp(float3(1.,.72,.26),float3(.3,.72,1.),saturate(Hue)),float3(.85,.92,1.),saturate(Hue-1.))*g*B;'
    out(custom(m, code, {'UV': node(m, unreal.MaterialExpressionTextureCoordinate), 'B': node(m, unreal.MaterialExpressionPerInstanceCustomData, data_index=0), 'Hue': node(m, unreal.MaterialExpressionPerInstanceCustomData, data_index=1)}), P.MP_EMISSIVE_COLOR)


MIST = NOISE + r'''
float r=saturate(1-length(UV-.5)*2.);
float n=F.B(UV*3.+float2(T*.02,Seed*7.));
Op=smoothstep(0.,1.,r)*smoothstep(.2,.75,n)*A;
return float3(.42,.54,.66);
'''


def mist(m):
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT); m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT); m.set_editor_property('two_sided', True)
    c = custom(m, MIST, {'UV': node(m, unreal.MaterialExpressionTextureCoordinate), 'T': node(m, unreal.MaterialExpressionTime), 'A': node(m, unreal.MaterialExpressionPerInstanceCustomData, data_index=0),
                         'Seed': node(m, unreal.MaterialExpressionPerInstanceCustomData, data_index=1)}, extra=[('Op', F1)])
    fade = node(m, unreal.MaterialExpressionDepthFade, fade_distance_default=180.); link(c, fade, 'Opacity', 'Op')
    out(c, P.MP_EMISSIVE_COLOR); out(fade, P.MP_OPACITY)


mats = {}
for name, fn in (('M_FeySky', sky), ('M_FeyGround', ground), ('M_FeyBark', bark), ('M_FeyPlant', plant), ('M_FeyWater', water), ('M_FeyRiver', river), ('M_FeyRoot', lambda m: bark(m, ROOT)), ('M_FeyFall', fall), ('M_FeyMote', mote), ('M_FeyMist', mist),
                 ('M_FeyLeaf', lambda m: foliage(m, nat('T_island_tree_02_leaves_diff_2k'), nat('T_island_tree_02_leaves_nor_dx_2k'), nat('T_island_tree_02_leaves_arm_2k'), nat('T_island_tree_02_leaves_alpha_2k'), (.95, 1.02, .5), None, 2.2, 0, light=True)),
                 ('M_FeyGrass', lambda m: foliage(m, textures['grass_medium_02', 'diffuse'], textures['grass_medium_02', 'nor_dx'], textures['grass_medium_02', 'arm'], textures['grass_medium_02', 'alpha'], (.62, .88, .42), None, 1.6, 22.)),
                 ('M_FeyCelandine', lambda m: foliage(m, textures['celandine_01', 'diffuse'], textures['celandine_01', 'nor_dx'], textures['celandine_01', 'arm'], textures['celandine_01', 'alpha'], (1., 1., .8), None, .8, 18.)),
                 ('M_FeyFungus', lambda m: fungus(m, 30.)), ('M_FeyFungusBig', lambda m: fungus(m, 64.)),
                 ('M_FeyBlade', lambda m: blade_mat(m, (.34, .46, .12), 9.)), ('M_FeyBladeRed', lambda m: blade_mat(m, (.62, .035, .09), 8.)),
                 ('M_FeyPetal', petal_mat), ('M_FeyPad', pad_mat), ('M_FeyFar', far), ('M_FeyRipple', ripple),
                 ('M_FeyMoss', lambda m: foliage(m, textures['moss_01', 'diffuse'], textures['moss_01', 'nor_dx'], textures['moss_01', 'arm'], textures['moss_01', 'alpha'], (.85, 1.1, .4), None, 0., 5.))):
    material(name, fn)

# ---------------------------------------------------------------- the kit
if MESHES:
    SLOTS = {'FeyBark': 'M_FeyBark', 'FeyLeaf': 'M_FeyLeaf', 'FeyPlant': 'M_FeyPlant', 'FeyFungus': 'M_FeyFungus', 'FeyFungusBig': 'M_FeyFungusBig', 'FeyBlade': 'M_FeyBlade', 'FeyBladeRed': 'M_FeyBladeRed', 'FeyPetal': 'M_FeyPetal', 'FeyPad': 'M_FeyPad', 'FeyFar': 'M_FeyFar', 'grass_medium_02': 'M_FeyGrass', 'celandine_01': 'M_FeyCelandine', 'moss_01': 'M_FeyMoss'}
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    LODS = {'SM_FeyOak0': [(1, 1.), (.45, .5), (.16, .22), (.05, .08)], 'SM_FeyOak1': [(1, 1.), (.45, .5), (.16, .22), (.05, .08)], 'SM_FeyCapViolet': [(1, 1.), (.3, .12)], 'SM_FeyCapBlue': [(1, 1.), (.3, .1)],
            'SM_FeyCapAmber': [(1, 1.), (.5, .1)], 'SM_FeyFoxglove': [(1, 1.), (.35, .14)], 'SM_FeyBroadLeaf': [(1, 1.), (.4, .14)], 'SM_FeyCrimsonLeaf': [(1, 1.), (.4, .14)], 'SM_FeyLily': [(1, 1.), (.4, .1)], 'SM_FeyLilyPad': [(1, 1.), (.5, .035)], 'SM_FeyFarTree': [(1, 1.), (.45, .03)]}
    derived = SOURCE / 'Derived'
    for rec in json.loads((derived / 'manifest.json').read_text(encoding='utf-8'))['assets']:
        file = derived / rec['file']
        if MESH_ONLY and file.stem not in MESH_ONLY: continue
        if hashlib.sha256(file.read_bytes()).hexdigest().upper() != rec['sha256']: raise RuntimeError(file.name + ' hash')
        task = unreal.AssetImportTask(); task.filename = str(file); task.destination_path = DEST; task.destination_name = file.stem; task.automated = True; task.replace_existing = True; task.save = False
        o = unreal.FbxImportUI(); o.import_mesh = True; o.import_as_skeletal = False; o.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH; o.automated_import_should_detect_type = False
        o.import_materials = False; o.import_textures = False; d = o.static_mesh_import_data; d.combine_meshes = True; d.auto_generate_collision = False
        d.vertex_color_import_option = unreal.VertexColorImportOption.REPLACE; d.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
        task.options = o; TOOLS.import_asset_tasks([task]); mesh = unreal.load_asset(f'{DEST}/{file.stem}')
        if not mesh: raise RuntimeError('Import failed: ' + file.stem)
        for i, slot in enumerate(mesh.static_materials):
            label = str(slot.material_slot_name).split('.')[0]
            if label not in SLOTS: raise RuntimeError(f'Unexpected slot {label} on {file.stem}')
            mesh.set_material(i, unreal.load_asset(f'{DEST}/{SLOTS[label]}'))
        if file.stem.endswith('Collision'):
            mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        if file.stem in LODS:
            opts = unreal.StaticMeshReductionOptions(); opts.auto_compute_lod_screen_size = False; levels = []
            for percent, screen in LODS[file.stem]:
                s = unreal.StaticMeshReductionSettings(); s.percent_triangles = percent; s.screen_size = screen; levels.append(s)
            opts.reduction_settings = levels; subsystem.set_lods(mesh, opts)
        ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
        unreal.log(f'CHUCK_FEY_MESH {file.stem} triangles={rec["triangles"]} lods={mesh.get_num_lods()}')
unreal.log('CHUCK_FEY_ASSETS_DONE')
