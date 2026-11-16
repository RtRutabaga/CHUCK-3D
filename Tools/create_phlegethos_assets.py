"""Phlegethos's textures, scores and materials (Claude, 2026-11-14; docs/regions/PHLEGETHOS.md).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=<absolute path to this file> -AllowCommandletAudio -unattended -nosplash -NoLiveCoding
(-run=pythonscript crashes in the content browser when importing.)

Imports the CC0 maps fetched by Tools/Fetch-Phlegethos.ps1 (SourceAssets/Setting/Phlegethos) into
/Game/Art/Phlegethos, the user's two Phlegethos tracks (SourceAssets/Audio/Phlegethos.wav, Phlegethos2.wav, unmodified)
into /Game/Art/Audio as SW_Phlegethos and SW_Phlegethos2 (looping), and builds:

  M_PhlegBasalt  opaque basalt for the procedural stage (triplanar on local position, which is the stage frame: Z is the
                 height above the lava). Vertex colour R: cracked heat (glowing seams in the rock's dark creases), G: the
                 second basalt, B: ash settled on top. The lava's light comes up the faces near it (Z under about 2 m).
  M_PhlegPath    the worn volcanic flagstones of the road and the fortress, with ash in the joints.
  M_PhlegLava    the molten surface: the lake's rounded crust plates on a net of glowing veins, open molten pools in places
                 (Lava005's swirls), the plates crazed and, past 60 m, the crust scan (ambientCG Lava001); the flows and
                 falls stream with Lava005. UV V runs down a flow (rivers, falls) and vertex colour G is its speed.
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
    tex.set_editor_property('srgb', role in ('diffuse', 'emit'))
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP if role == 'nor_dx' else
                            unreal.TextureCompressionSettings.TC_DEFAULT if role in ('diffuse', 'emit') else unreal.TextureCompressionSettings.TC_MASKS)
    ASSETS.save_loaded_asset(tex, only_if_is_dirty=False)
    textures[rec['asset'], role] = tex


def tex_object(m, asset, role):
    typ = {'diffuse': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, 'emit': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, 'nor_dx': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
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
// Plates: the up-facing basalt broken into slabs, the lava glowing in the cracks between them where the vertex colour
// says the ground is hot. (2026-11-16, the user's goal image References/Phlegethos/phlegethos-goal-hot-cracks.png:
// broad dark plates, two to three metres, their edges ragged and bevelled down into the cracks; the cracks a hand wide,
// glowing all along with a yellow core and flickering fire in them; the plates' lips lit orange by them; fine dark
// hairline cracks across the plates. The first plates were 1.6 m with hairline glowing cracks, many of them cooled
// dark, and read as straight lines drawn on a floor.)
float hot=smoothstep(.12,.55,VC.r)*smoothstep(.55,.85,n.z)*saturate(1-ash*1.2);
float wob=F.N(P.xy/45);
float3 lit=0;
if(hot>.003)
{
    // The crack's distance at a point: warped twice so the plates are irregular, ragged at small scale.
    float2 w1=float2(F.N(P.xy/360),F.N(P.xy/360+9.1))*.5,w2=float2(F.N(P.xy/90+4.2),F.N(P.xy/90+7.7))*.07;
    float2 pq=P.xy/250+w1+w2;
    float rag=(F.N(P.xy/7)-.5)*.018+(F.N(P.xy/19+2.3)-.5)*.026;
    float e=F.Edge(pq)+rag;
    // Its gradient (which way the plate's lip falls toward the crack), by differences two centimetres over.
    float ex=F.Edge(pq+float2(2./250,0))+rag,ey=F.Edge(pq+float2(0,2./250))+rag;
    float2 gd=float2(ex-e,ey-e);gd/=max(length(gd),1e-5);
    // The width: 3 to 8 cm, wider in places; a few runs near shut.
    // (First pass .016-.058 with a strong glow: the cracks bloomed into bands and the exposure blacked out the plates.)
    float wd=.011+.018*F.N(pq*.8+1.3)+.006*wob;
    float run=smoothstep(.12,.42,F.N(pq*.55+3.7)*.8+wob*.3);
    wd*=lerp(.5,1,run);
    float gap=1-smoothstep(wd*.7,wd,e);
    float core=1-smoothstep(0,wd*.55,e);
    // Fire licking in the cracks, moving along them.
    float lick=F.N(P.xy/9+T*float2(.35,1.2))*.65+F.N(P.xy/23-T*float2(.6,.2))*.35;
    float flame=gap*(.55+.9*lick);
    // The lip: bevelled down into the crack over 6 to 10 cm, lit by it.
    float lipW=wd+.035;
    float lip=(1-smoothstep(wd,lipW,e))*(1-gap);
    float glowLip=exp(-max(e-wd,0)/.011)*(1-gap);
    // Hairline cracks across the plates, dark, a few glowing faintly.
    float e2=F.Edge(pq*2.6+w1*2.+5.3)+rag*.6;
    float hair=(1-smoothstep(.006,.016,e2))*(1-gap)*smoothstep(wd+.04,wd+.09,e);
    float hairHot=hair*smoothstep(.72,.9,F.N(pq*3.1+1.7));
    rock*=lerp(1,1-.65*hair*(1-hairHot),hot);
    rock=lerp(rock,rock*.75+float3(.03,.012,.006),lip*hot);
    rock=lerp(rock,float3(.02,.008,.004),gap*hot);
    // The bevel: the normal tilted toward the crack on the lip (world XY: the stage is never turned).
    n=normalize(n+float3(-gd*lip*hot*.9,0));
    lit=float3(1.,.62,.16)*core*hot*run*(3.5+3*lick)+float3(1.,.3,.05)*flame*hot*(1.6+1.6*run)
       +float3(.95,.22,.035)*glowLip*hot*(.25+.5*run)+float3(1.,.3,.05)*hairHot*hot*.3;
}
Emit=float3(1.,.28,.04)*(seam*seam*22*flick)+float3(.85,.16,.03)*under*(.6+.4*lum*3)+lit;
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
    float L(float3 c){return dot(c,float3(.3,.59,.11));}
    float Edge(float2 p)
    {
        float2 i=floor(p),f=frac(p),mr=0;float md=8;
        for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){float2 g=float2(x,y),o=float2(H(i+g),H(i+g+17.3))*.8+.1,r=g+o-f;float d=dot(r,r);if(d<md){md=d;mr=r;}}
        // Smooth minimum of the distances to the cell's borders: the plates' corners round off and the veins pool at
        // their junctions, as in the goal image (a hard minimum gave straight-sided polygons).
        float sm=0;
        for(int y2=-2;y2<=2;y2++)for(int x2=-2;x2<=2;x2++){float2 g=float2(x2,y2),o=float2(H(i+g),H(i+g+17.3))*.8+.1,r=g+o-f;if(dot(mr-r,mr-r)>.00001)sm+=exp(-14*dot(.5*(mr+r),normalize(r-mr)));}
        return max(0,-log(sm)/14);
    }
};
LavaFns F;
// (2026-11-16, with ambientCG CC0 scans: Lava001 for the lake - dark crust torn open by molten streaks - and Lava005 for
// the flows - molten lines streaming. The procedural Voronoi plates read as a regular hex net from Chuck's eye and above.)
// UVs are world cm/400; vertex colour G is the flow's speed (0 the lake .. 1 a fall), V runs down a flow.
float speed=VC.g;
// Wrapped so the noise hash keeps its precision far out on the lake (UVs reach thousands there).
float2 uv=UV-floor(UV/512)*512;
float2 warp=float2(F.N(uv*.9+T*.03),F.N(uv*.9+5.2-T*.025))-.5;
// The lake (2026-11-16, the user's goal image References/Phlegethos/phlegethos-goal-lava-columns.png): rounded crust
// plates about 1.5 m across, dark red-brown and faintly glowing, drifting apart on a branching net of bright molten veins
// that run wide in places; the plate rims glow from the veins, and fine cracks (the Lava001 scan) craze their tops. Warped
// twice so the cells are rounded and never read as a hex net.
float2 a1=uv*.28+warp*.18+float2(T*.004,-T*.006);
float2 a2=float2(uv.x*.7-uv.y*.71,uv.x*.71+uv.y*.7)*.16-warp*.12+float2(-T*.003,T*.002)+.31;
float e1=F.L(Texture2DSample(CE,CESampler,a1).rgb),e2=F.L(Texture2DSample(CE,CESampler,a2).rgb);
float3 cA=Texture2DSample(CA,CASampler,a1).rgb;
float2 pq=uv*2.6+warp*1.2+float2(T*.006,-T*.004);
pq+=(float2(F.N(pq*.55+1.7),F.N(pq*.55+8.3))-.5)*.9+(float2(F.N(pq*.13+2.9),F.N(pq*.13+6.1))-.5)*1.6;
float e=F.Edge(pq);
float wide=smoothstep(.3,.85,F.N(pq*.21+3.1)*.75+F.N(pq*.9+4.4)*.25);
float wid=.012+.11*wide*wide;
// Open pools (2026-11-16, the user's second goal image References/Phlegethos/phlegethos-goal-lava-fall.png): in places
// the plates shrink to rafts and give way to bright open melt crossed by thin swirling lines (Lava005, drifting).
float open=smoothstep(.55,.82,F.N(uv*.03+float2(T*.002,0))*.62+F.N(uv*.1-T*.004)*.38);
wid+=open*open*.55;
float vein=(1-smoothstep(wid*.25,wid,e))*(.72+.28*smoothstep(.02,.12,wid-e));
float2 sw=float2(uv.y*.7+uv.x*.3,uv.x*.7-uv.y*.3)*.32+warp*.45+float2(T*.006,-T*.01);
float swirl=F.L(Texture2DSample(FE,FESampler,sw).rgb);
float pool=saturate(.5+pow(saturate(swirl*1.5),1.4)*.55);
float rim=exp(-e*(16-7*wide))*(1-vein);
float craze=saturate(e1*2.4-.3);
float lakeHeat=max(vein*lerp(.7+.3*F.N(pq*1.7-T*.2),pool,open),max(rim*.45,craze*.2));
// The veins' detail fades before it shimmers (a 1.5 m net at 100 m is a few pixels); the scans carry the middle distance.
float vf=smoothstep(6000,20000,D);
lakeHeat=lerp(lakeHeat,saturate(.18+e2*1.1+e1*.4),vf);
// The flows: the streaming scan along V at the flow's speed, two layers sliding past each other.
float2 f1=float2(uv.y*.32-T*(.03+speed*.55),uv.x*.55);
float2 f2=float2(uv.y*.55-T*(.05+speed*.85),uv.x*.9+.43);
float s1=F.L(Texture2DSample(FE,FESampler,f1).rgb),s2=F.L(Texture2DSample(FE,FESampler,f2).rgb);
// (The second goal image: a fall nearly all molten, orange to yellow, the darker red streaks running down it.)
float flowHeat=saturate(.3+pow(saturate(max(s1,s2*.85)*1.45),1.35)*.82);
float k=smoothstep(.08,.35,speed);
float molten=lerp(lakeHeat,flowHeat,k);
// Far off the scans blur to their average: a dull glow with broad hot patches, so the lake from the fall reads molten.
// (Fully at the horizon: any of the scans left there showed as a grid of tiles.)
// Each finer term fades out before its cells shrink to pixels (they shimmered into a grid toward the horizon); the flows
// keep their heat, so the rivers still read from the crane.
float far=smoothstep(6000,70000,D);
float m1=1-smoothstep(150000,600000,D),m2=1-smoothstep(40000,150000,D);
float macro=lerp(.45,smoothstep(.4,.9,F.N(uv*.004+.7)*.6+lerp(.5,F.N(uv*.013+T*.001),m2)*.4),m1);
// From the fall's height two coarse layers of the crust scan (about 400 m and 1 km, turned) keep the lake broken into
// rafts and glowing rifts; the near scans blur to their average there.
float2 a3=float2(uv.x*.6+uv.y*.8,uv.y*.6-uv.x*.8)*.01+warp*.01+float2(T*.0002,0);
float2 a4=float2(uv.x*.8-uv.y*.6,uv.y*.8+uv.x*.6)*.0039+.53;
float e3=max(F.L(Texture2DSample(CE,CESampler,a3).rgb),F.L(Texture2DSample(CE,CESampler,a4).rgb)*.9);
float farLake=.06+.08*lerp(.3,open,m2)+.2*macro*macro+m2*smoothstep(.1,.42,e3)*.7;
molten=lerp(molten,lerp(farLake,.42,k),far);
// (A slow throb, out of phase across the lake: a sine of the UVs drew 18 m diagonal stripes, seen from the fall.)
float pulse=.88+.12*sin(T*1.3+F.N(uv*.05)*12*m2);
float3 hot=lerp(float3(.65,.05,.005),float3(1.,.28,.03),saturate(molten*1.2))*pulse;
hot=lerp(hot,float3(1.,.6,.22),saturate((molten-.9)*6));
Emit=hot*molten*(2+8*molten*molten)*VA*(1-.4*k);
// The plates themselves smoulder dull red (the goal's crust is never black), more where they are thin.
Emit+=float3(.16,.018,.003)*(.2+.8*F.N(pq*.35+T*.05))*(1-molten)*(1-k)*(1-vf*.6)*VA;
Rough=lerp(.55,.35,molten);
float3 Nn=lerp(Texture2DSample(CN,CNSampler,a1).rgb,Texture2DSample(FN,FNSampler,f1).rgb,k)*2-1;
WN=normalize(float3(Nn.xy*(1-molten)*1.2,1));
// (Molten nearly black in base colour: its light is emissive. Lit, the bright base colour showed the sun's shadow acne
// as diagonal stripes from above.)
float3 crustCol=lerp(cA*.2,float3(.05,.022,.016),.5);
return lerp(crustCol,float3(.03,.008,.002),saturate(molten*1.5));
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
              'CA': tex_object(m, 'Lava001', 'diffuse'), 'CE': tex_object(m, 'Lava001', 'emit'),
              'CN': tex_object(m, 'Lava001', 'nor_dx'), 'FE': tex_object(m, 'Lava005', 'emit'), 'FN': tex_object(m, 'Lava005', 'nor_dx')}
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
