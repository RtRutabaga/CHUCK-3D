"""The Deepwood jellyfish's material (Claude, 2026-11-26; docs/regions/FEYWILD-JELLYFISH.md).

UnrealEditor-Cmd <uproject> "-ExecutePythonScript=<absolute path to this file>" -unattended -nosplash -NoLiveCoding

  M_FeyJelly   translucent and unlit, two-sided: the bell, its frills and its tendrils (all procedural meshes built in
               FeyJelly.cpp). The colour is the mesh's vertex colour times Tint, glowing more toward the silhouette
               (a fresnel) and with the pulse (Glow, set by the game). Vertex alpha adds to the opacity: the rim and the
               tendrils are denser than the thin dome. Fogged as translucency is, so it fades into the Deepwood's fog.
Only this asset is created or replaced. To rebuild it, delete the .uasset first (clearing a loaded material's graph
crashes UE 5.7).
"""
import unreal

DEST = '/Game/Art/Feywild/Creatures/Jelly'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
LIB = unreal.MaterialEditingLibrary
ASSETS.make_directory(DEST)
F1, F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3

CODE = r"""
float f=saturate(F);
float3 c=VC.rgb*Tint.rgb;
float glow=(Glow*(.04+.26*f*f)+VA*.30)*Bright;   // (the Deepwood is exposed bright: a little emission reads as a glow; the painted detail glows most)
Opa=saturate(.03+.22*f*f+VA*.55)*Fade;   // (the silhouette only a little denser: the canals, rim lights and marks carry it)
return c*glow;
"""


def build():
    path = f'{DEST}/M_FeyJelly'
    if ASSETS.does_asset_exist(path):
        print('CHUCK_FEY_JELLY_MATERIAL kept (delete it to rebuild)')
        return
    mat = TOOLS.create_asset('M_FeyJelly', DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided', True)

    def node(cls, **props):
        e = LIB.create_material_expression(mat, cls)
        for k, v in props.items(): e.set_editor_property(k, v)
        return e
    vc = node(unreal.MaterialExpressionVertexColor)
    fr = node(unreal.MaterialExpressionFresnel, exponent=2.2, base_reflect_fraction=0.)
    tint = node(unreal.MaterialExpressionVectorParameter, parameter_name='Tint', default_value=unreal.LinearColor(.35, .95, 1., 1.))
    glow = node(unreal.MaterialExpressionScalarParameter, parameter_name='Glow', default_value=1.)
    fade = node(unreal.MaterialExpressionScalarParameter, parameter_name='Fade', default_value=1.)
    bright = node(unreal.MaterialExpressionScalarParameter, parameter_name='Bright', default_value=1.)
    c = node(unreal.MaterialExpressionCustom, code=CODE, output_type=F3, description='FeyJelly')
    ins = []
    for n in ('VC', 'VA', 'F', 'Tint', 'Glow', 'Fade', 'Bright'):
        i = unreal.CustomInput(); i.set_editor_property('input_name', n); ins.append(i)
    c.set_editor_property('inputs', ins)
    o = unreal.CustomOutput(); o.set_editor_property('output_name', 'Opa'); o.set_editor_property('output_type', F1)
    c.set_editor_property('additional_outputs', [o])
    for expr, pin, name in ((vc, '', 'VC'), (vc, 'A', 'VA'), (fr, '', 'F'), (tint, '', 'Tint'), (glow, '', 'Glow'), (fade, '', 'Fade'), (bright, '', 'Bright')):
        if not LIB.connect_material_expressions(expr, pin, c, name): raise RuntimeError(f'M_FeyJelly: input {name}')
    Pm = unreal.MaterialProperty
    if not LIB.connect_material_property(c, '', Pm.MP_EMISSIVE_COLOR): raise RuntimeError('M_FeyJelly: emissive')
    if not LIB.connect_material_property(c, 'Opa', Pm.MP_OPACITY): raise RuntimeError('M_FeyJelly: opacity')
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    print('CHUCK_FEY_JELLY_MATERIAL built')


build()
