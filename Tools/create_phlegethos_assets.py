"""Phlegethos's textures, scores and materials (Claude, 2026-11-14; docs/regions/PHLEGETHOS.md).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=<absolute path to this file> -AllowCommandletAudio -unattended -nosplash -NoLiveCoding
(-run=pythonscript crashes in the content browser when importing.)

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
  M_PhlegSky     the smoke sky dome (unlit); parameters Flash and FlashDir light its clouds round a lightning bolt.
  M_PhlegBolt    additive lightning ribbon (UV V across it, vertex colour R dims branches; parameter Intensity).
  M_PhlegMolten  unlit molten gobs on spheres: the geysers' fountain, the bombs and their marks, the fire rain.

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
    // (A sin-free hash: the sin one broke down along the cell lines into bright crosses on the road, 2026-11-15.)
    float H(float2 i){float3 p=frac(float3(i.xyx)*.1031);p+=dot(p,p.yzx+33.33);return frac((p.x+p.y)*p.z);}
    float N(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(H(i),H(i+float2(1,0)),f.x),lerp(H(i+float2(0,1)),H(i+1),f.x),f.y);}
    // Distance to the nearest edge between Voronoi cells (the plates' cracks), in cell units.
    float Edge(float2 p)
    {
        float2 i=floor(p),f=frac(p),mr=0;float md=8;
        for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){float2 g=float2(x,y),o=float2(H(i+g),H(i+g+17.3))*.85+.075,r=g+o-f;float d=dot(r,r);if(d<md){md=d;mr=r;}}
        md=8;
        for(int y2=-2;y2<=2;y2++)for(int x2=-2;x2<=2;x2++){float2 g=float2(x2,y2),o=float2(H(i+g),H(i+g+17.3))*.85+.075,r=g+o-f;if(dot(mr-r,mr-r)>.00001)md=min(md,dot(.5*(mr+r),normalize(r-mr)));}
        return md;
    }
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
float3 rock=lerp(a,b,b2)*float3(.58,.55,.56);
float lum=dot(rock,float3(.3,.59,.11));
float3 Na=F.TriN(AN,ANSampler,P,260,sg,w,n),Nb=F.TriN(BN,BNSampler,P,330,sg,w,n);
float3 arm=lerp(F.Tri(AA,AASampler,P,260,sg,w),F.Tri(BA,BASampler,P,330,sg,w),b2);
// Ash on whatever faces the sky, thicker in the hollows.
// (In drifts, not a blanket: all over the tops it smothered the plates' glowing cracks.)
float ash=saturate(VC.b*.7+smoothstep(.55,.95,n.z)*smoothstep(.5,.85,F.N(P.xy/380)+F.N(P.xy/90)*.3)*.7)*smoothstep(.2,.6,n.z);
float3 ashc=float3(.19,.18,.175)*(.8+.4*F.N(P.xy/40));
rock=lerp(rock,ashc,ash*.45);
// Seams of heat: the deepest creases (the scan's own occlusion, not its colour: this basalt is dark all over) glow where
// the vertex colour says the rock is cracked through.
float crease=saturate((.62-arm.r)*4.)*saturate(1-ash);
float flick=.75+.25*sin(T*1.7+P.x*.013+P.y*.017);
// (On the steep faces only: on the tops the scan's tile edges are dark in its occlusion and glowed as bright crosses at
// every tile corner, 2026-11-15; the plates' cracks do the tops.)
float seam=crease*VC.r*(1-smoothstep(.45,.75,n.z));
// The lava's light thrown up the faces close over it (a band at the foot only: stronger, rocks in the lake glowed gold).
float under=pow(saturate(1-P.z/160),2)*saturate(1.2-n.z)*.35;
// Plates: the up-facing basalt broken into slabs (about 1.6 m, warped so they never tile), the lava glowing in the cracks
// between them where the vertex colour says the ground is hot (the goal art's cracked plain).
float2 pq=P.xy/160+float2(F.N(P.xy/310),F.N(P.xy/310+9.1))*.55;
float e=F.Edge(pq),wob=F.N(P.xy/45);
float hot=smoothstep(.12,.55,VC.r)*smoothstep(.55,.85,n.z)*saturate(1-ash*1.2);
// (Thin, and hot only along some runs: some cracks cooled dark, as the goal art's plain.)
float run=smoothstep(.35,.7,F.N(pq*.6+3.7)*.8+wob*.3);
float gap=1-smoothstep(.008+.008*wob,.022+.012*wob,e);
float crack=gap*hot*run;
float rim=(1-smoothstep(.02,.07,e))*hot;
rock*=lerp(1,.5,rim);
rock=lerp(rock,float3(.015,.01,.008),gap*hot);
Emit=float3(1.,.28,.04)*(seam*seam*22*flick)+float3(.85,.16,.03)*under*(.6+.4*lum*3)
    +float3(1.,.28,.04)*crack*(7+5*flick*wob)+float3(.6,.1,.015)*saturate(rim-gap)*run*.5;
WN=normalize(n+lerp(Na,Nb,b2)*(1-ash*.6)*1.15);
// (A glassy sheen on the basalt, as the goal art's: the lava's glow catches its edges.)
AO=arm.r; Rough=lerp(lerp(.5,.78,arm.g),.92,ash);
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
float under=pow(saturate(1-P.z/160),2)*saturate(1.2-n.z)*.35;
Emit=float3(.85,.16,.03)*under+float3(1,.3,.05)*saturate(VC.r*joint*2)*10;
WN=normalize(n+Nn*(1-ash*.5));
AO=arm.r; Rough=lerp(.78,.95,ash);
return c;
'''

LAVA = r'''
struct LavaFns
{
    float H(float2 i){return frac(sin(dot(i,float2(127.1,311.7)))*43758.5453);}
    float N(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(H(i),H(i+float2(1,0)),f.x),lerp(H(i+float2(0,1)),H(i+1),f.x),f.y);}
    float Edge(float2 p)
    {
        float2 i=floor(p),f=frac(p),mr=0;float md=8;
        for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){float2 g=float2(x,y),o=float2(H(i+g),H(i+g+17.3))*.85+.075,r=g+o-f;float d=dot(r,r);if(d<md){md=d;mr=r;}}
        md=8;
        for(int y2=-2;y2<=2;y2++)for(int x2=-2;x2<=2;x2++){float2 g=float2(x2,y2),o=float2(H(i+g),H(i+g+17.3))*.85+.075,r=g+o-f;if(dot(mr-r,mr-r)>.00001)md=min(md,dot(.5*(mr+r),normalize(r-mr)));}
        return md;
    }
};
LavaFns F;
// World-scale UVs (cm/400): drifting slowly, the flow's speed down V (vertex colour G, 0 still .. 1 a fall).
// (2026-11-15, the user's goal art: deep red-orange lava under a dark drifting crust, molten in its seams and its rivers;
// the yellow-white only in the hottest threads. The flat bright sheet lit everything round it tan.)
float speed=VC.g;
float2 uv=UV;
float2 warp=float2(F.N(uv*1.3+T*.05),F.N(uv*1.3+5.2-T*.04))-.5;
float2 u1=uv*.55+warp*.35+float2(T*.006,-T*(.012+speed*.9));
float2 u2=uv*1.1-warp*.25+float2(-T*.009,-T*(.02+speed*1.4))+.37;
float3 c1=Texture2DSample(LD,LDSampler,u1).rgb,c2=Texture2DSample(LD,LDSampler,u2).rgb;
float l1=dot(c1,float3(.3,.59,.11)),l2=dot(c2,float3(.3,.59,.11));
float scan=saturate(max(l1,l2*.9)*1.6-.5+(F.N(uv*2.5+T*.15)-.5)*.3);
// Crust plates (about 2.5 m) drifting on the flow, cracked apart; molten between them, the plates dark with a dull ember
// heart where they are thin.
float2 pq=uv*1.6+warp*.6+float2(T*.004,-T*(.008+speed*.6));
float e=F.Edge(pq),thin=F.N(pq*2.3+7.1);
// (Seams thin, and hot only in places: some cracks cooled dark, so the plates never read as an even net.)
float seamHeat=smoothstep(.3,.75,F.N(pq*.45+T*.02)*.75+thin*.35);
float seam=(1-smoothstep(.01,.05+.05*thin,e))*(1-speed)*seamHeat;
float crust=(1-seam)*smoothstep(.25,.55,thin*.6+F.N(uv*.7-T*.01)*.6)*(1-speed);
float molten=saturate(max(scan*(1-crust*.9),seam*(.5+.3*thin)));
// A fall or river: streaks down the flow, molten with dark crust threads carried in it.
float streak=F.N(float2(uv.x*7,uv.y*.5-T*speed*1.6))*.7+F.N(float2(uv.x*19,uv.y*1.4-T*speed*2.2))*.3;
molten=lerp(molten,saturate(.22+streak*.7),speed);
// Far off, the seams merge into a dull glow: the lake seen from the fall or across it reads molten, not as sand.
float far=saturate((D-2500)/40000);
molten=lerp(molten,.16+.12*molten+.22*smoothstep(.45,.85,F.N(uv*.03+T*.002)),far*.8);
float pulse=.85+.15*sin(T*1.3+uv.x*2.1+uv.y*1.7);
float3 hot=lerp(float3(.7,.06,.006),float3(1.,.3,.035),saturate(molten*1.25))*pulse;
hot=lerp(hot,float3(1.,.62,.25),saturate((molten-.9)*6));
Emit=hot*molten*(3+8*molten*molten)*VA*(1-.6*speed);
Rough=lerp(.85,.3,molten);
float3 Nn=Texture2DSample(LN,LNSampler,u1).rgb*2-1;
WN=normalize(float3(Nn.xy*(1-molten)*1.1,1));
return lerp(float3(.025,.018,.016),float3(.22,.05,.01),molten);
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


SKY = r'''
struct SkyFns
{
    float H(float2 i){return frac(sin(dot(i,float2(127.1,311.7)))*43758.5453);}
    float N(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(H(i),H(i+float2(1,0)),f.x),lerp(H(i+float2(0,1)),H(i+1),f.x),f.y);}
    float Fbm(float2 p){float s=0,a=.5;for(int k=0;k<6;k++){s+=a*N(p);p=p*2.03+float2(1.7,9.2);a*=.5;}return s;}
};
SkyFns F;
// A sky of smoke: heavy billows drifting, black above, lit from below by the burning land - deep red at the horizon,
// orange toward the volcano. Seen on a dome round the stage.
float3 d=normalize(Dir);
float up=saturate(d.z);
float2 uv=d.xy/(up+.12)*1.4;
float t=T*.006;
float w=F.Fbm(uv*.6+float2(t,-t*.7));
float c=F.Fbm(uv*1.2+w*1.6+float2(-t*1.3,t));
float billow=smoothstep(.3,.75,c);
float horizon=pow(1-up,3.);
float2 vd=normalize(Volc);
float toward=pow(saturate(dot(normalize(d.xy+.0001),vd)),5)*pow(1-up,1.2);
// (2026-11-15, seen at last past the haze and the ranges: dark. The smoke's undersides lit red by the burning land, low
// down and toward the volcano orange; the gaps between the billows darker; black overhead.)
float3 lit=lerp(float3(.3,.03,.007),float3(.8,.13,.02),toward);
float shade=lerp(.22,1.,billow)*(.75+.5*w);
float3 col=float3(.004,.0015,.001)+lit*(horizon*.9+.16*(1-up))*shade;
col+=float3(1,.26,.04)*toward*horizon*.45*billow;
// At the horizon and under it (seen from the fall's height, past the lava's edge): a haze glowing from the land, not the
// cloud pattern, which stretched into streaks there.
float3 haze=lit*(.75+.25*toward)*lerp(1.,.55,saturate(-d.z*6));
col=lerp(haze,col,smoothstep(0.,.07,d.z));
// Hell lightning (2026-11-15): the smoke lit from inside round the bolt, the billows brightest (Flash 0..1 from the stage).
float k=saturate(dot(d,normalize(FD.xyz)));
float fl=(pow(k,90)*2.5+pow(k,14)*.7+pow(k,3)*.08)*(.35+billow*.9+.3*w);
col+=lerp(float3(1.,.16,.07),float3(1.,.5,.4),pow(k,90))*fl*Flash*2.5;
return col*Bright;
'''

BOLT = r'''
// A lightning bolt's ribbon: UV V across it (0..1), a white-hot core in a crimson glow; VC.r dims the branches.
float a=1-abs(UV.y*2-1);
float core=pow(a,7),glow=pow(a,1.8);
float3 c=float3(1.,.12,.05)*glow*.35+float3(1.,.62,.5)*core*2.5;
return c*VC.r*Intensity;
'''


def sky(m):
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('is_sky', True)
    wp = node(m, unreal.MaterialExpressionWorldPosition)
    cam = node(m, unreal.MaterialExpressionCameraPositionWS)
    dirn = node(m, unreal.MaterialExpressionSubtract)
    link(wp, dirn, 'A'); link(cam, dirn, 'B')
    v = node(m, unreal.MaterialExpressionConstant2Vector, r=.88, g=-.47)
    flash = node(m, unreal.MaterialExpressionScalarParameter, parameter_name='Flash', default_value=0.)
    fd = node(m, unreal.MaterialExpressionVectorParameter, parameter_name='FlashDir', default_value=unreal.LinearColor(1, 0, .3, 0))
    c = custom(m, SKY, {'Dir': dirn, 'T': node(m, unreal.MaterialExpressionTime), 'Volc': v, 'Bright': const(m, .45), 'Flash': flash, 'FD': fd})
    out(m, c, unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def bolt(m):
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property('two_sided', True)
    # (Unfogged: the stage's thick low haze swallowed every bolt more than a few hundred metres off; the stage fades them.)
    for prop in ('apply_fogging', 'apply_cloud_fogging'):
        try: m.set_editor_property(prop, False)
        except Exception as e: unreal.log_warning(f'CHUCK_PHLEG_BOLT_NOFOG {prop} {e}')
    vc = node(m, unreal.MaterialExpressionVertexColor)
    c = custom(m, BOLT, {'UV': node(m, unreal.MaterialExpressionTextureCoordinate), 'VC': vc,
                         'Intensity': node(m, unreal.MaterialExpressionScalarParameter, parameter_name='Intensity', default_value=0.)})
    out(m, c, unreal.MaterialProperty.MP_EMISSIVE_COLOR)


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
    out(m, const(m, .5), unreal.MaterialProperty.MP_SPECULAR)


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


MOLTEN = r'''
// A gob of molten rock (2026-11-16: the geysers' fountain, the bombs and their marks, the fire rain): hottest facing the
// eye, a darker crusted rim so it reads round, the heat moving over it. (On the lava material they came out half black,
// half white: its crust plates and UVs are for the lake, not a ball.)
float f=saturate(dot(normalize(N),normalize(CV)));
float wob=.5+.5*sin(P.x*.11+T*6.)*sin(P.y*.09-T*5.)*sin(P.z*.13+T*4.3);
float heat=saturate(.25+.75*pow(f,.6))*(.7+.3*wob);
float3 c=lerp(float3(.5,.04,.004),float3(1.,.34,.04),heat);
c=lerp(c,float3(1.,.7,.3),saturate((heat-.82)*4.));
return c*heat*S;
'''


def molten(m):
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('used_with_instanced_static_meshes', True)
    c = custom(m, MOLTEN, {'N': node(m, unreal.MaterialExpressionVertexNormalWS), 'CV': node(m, unreal.MaterialExpressionCameraVectorWS),
                           'P': node(m, unreal.MaterialExpressionWorldPosition), 'T': node(m, unreal.MaterialExpressionTime), 'S': const(m, 3.)})
    out(m, c, unreal.MaterialProperty.MP_EMISSIVE_COLOR)


material('M_PhlegBasalt', basalt)
material('M_PhlegPath', path)
material('M_PhlegLava', lava)
material('M_PhlegGlow', glow)
material('M_PhlegEmber', ember)
material('M_PhlegSky', sky)
material('M_PhlegBolt', bolt)
material('M_PhlegMolten', molten)
unreal.log(f'CHUCK_PHLEG_ASSETS_READY materials=8 textures={len(textures)}')
