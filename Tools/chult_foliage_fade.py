"""See-through underbrush for Chult (Claude, 2026-10-27; docs/CHULT-FOLIAGE-FADE.md).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/chult_foliage_fade.py -unattended -nosplash -NoLiveCoding

Leaves near Chuck, and leaves between the camera and him, are drawn partly see-through (a dither, about a third
of their leaf left) so thick undergrowth can be walked through and seen past. Their shadows stay whole.

Creates /Game/Art/ChultNature/MPC_ChultFoliageFade (Chuck: his position and an on/off in w; Eye: the camera),
which AChultFoliageFade (C++) sets every frame, and inserts the fade into the opacity mask of the jungle's leaf
materials. A material already carrying it (its mask fed by the fade's custom node) is left alone.

The materials' own builders (create_chult_wet_materials.py for M_FallLeaves, import_chult_nature.py for
M_ChultFern) call apply() at their end, so rebuilding them keeps it.
"""
import unreal

MPC_PATH = '/Game/Art/ChultNature/MPC_ChultFoliageFade'
TAG = 'CHUCK_FOLIAGE_FADE'
LIB = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MATERIALS = ['/Game/Art/Cutscenes/FallToChult/M_FallLeaves', '/Game/Art/ChultNature/M_ChultFern']

CODE = r'''
#if SHADOW_DEPTH_SHADER
return A;   // its shadow stays whole
#endif
// Where the leaf is, against Chuck (C) and the camera (E).
float3 C = Chuck.xyz, E = Eye.xyz;
float3 D = C - E;
float T = saturate(dot(P - E, D) / max(dot(D, D), 1.0));
float Line = length(P - (E + D * T));
// Between the camera and him: a 0.6-1.3 m tube along the line of sight.
float Between = 1.0 - smoothstep(60.0, 130.0, Line);
// Round him: the brush he is walking through (squashed upward, so a tall plant fades along its stem).
float Round = 1.0 - smoothstep(110.0, 210.0, length((P - C) * float3(1.0, 1.0, 0.6)));
// Only the undergrowth: nothing well above his head (crowns, the canopy).
float Low = 1.0 - smoothstep(C.z + 220.0, C.z + 420.0, P.z);
float Fade = saturate(max(Between, Round) * Low * Chuck.w);
if (Fade <= 0.0) return A;
// A temporal interleaved-gradient dither: anti-aliasing blends it into a soft see-through.
float2 Px = Parameters.SvPosition.xy + 5.588238 * float(View.StateFrameIndexMod8);
float Noise = frac(52.9829189 * frac(dot(Px, float2(0.06711056, 0.00583715))));
return Noise < 1.0 - Fade * 0.68 ? A : 0.0;
'''


def collection():
    mpc = unreal.load_asset(MPC_PATH)
    if mpc is None:
        mpc = TOOLS.create_asset('MPC_ChultFoliageFade', '/Game/Art/ChultNature', unreal.MaterialParameterCollection,
                                 unreal.MaterialParameterCollectionFactoryNew())
    params = []
    for name, value in (('Chuck', unreal.LinearColor(0, 0, -1.0e7, 0)), ('Eye', unreal.LinearColor(0, 0, -1.0e7, 0))):
        p = unreal.CollectionVectorParameter()
        p.set_editor_property('parameter_name', name)
        p.set_editor_property('default_value', value)
        params.append(p)
    existing = [str(p.get_editor_property('parameter_name')) for p in mpc.get_editor_property('vector_parameters')]
    if existing != ['Chuck', 'Eye']:
        mpc.set_editor_property('vector_parameters', params)
    unreal.EditorAssetLibrary.save_loaded_asset(mpc, only_if_is_dirty=False)
    return mpc


def apply(mat, mpc=None):
    """Insert the fade between mat's opacity-mask source and the mask (the shadow pass keeps the source)."""
    mpc = mpc or collection()
    prop = unreal.MaterialProperty.MP_OPACITY_MASK
    src = LIB.get_material_property_input_node(mat, prop)
    if src is None:
        raise RuntimeError(f'{mat.get_name()}: no opacity mask to fade')
    out_name = LIB.get_material_property_input_node_output_name(mat, prop)
    # Already faded (this script run twice on the same graph): left as it is. The builders start from an empty graph.
    if isinstance(src, unreal.MaterialExpressionCustom) and src.get_editor_property('desc') == TAG:
        unreal.log(f'{TAG} material={mat.get_name()} already=1')
        return
    args = []
    for name in ('A', 'P', 'Chuck', 'Eye'):
        a = unreal.CustomInput(); a.set_editor_property('input_name', name); args.append(a)
    custom = LIB.create_material_expression(mat, unreal.MaterialExpressionCustom, -700, 600)
    custom.set_editor_property('code', CODE)
    custom.set_editor_property('inputs', args)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    custom.set_editor_property('desc', TAG)
    LIB.connect_material_expressions(src, out_name or '', custom, 'A')
    LIB.connect_material_expressions(LIB.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1000, 600), '', custom, 'P')
    for name in ('Chuck', 'Eye'):
        cp = LIB.create_material_expression(mat, unreal.MaterialExpressionCollectionParameter, -1000, 700 if name == 'Chuck' else 800)
        cp.set_editor_property('collection', mpc)
        cp.set_editor_property('parameter_name', name)
        LIB.connect_material_expressions(cp, '', custom, name)
    LIB.connect_material_property(custom, '', prop)
    LIB.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False)
    unreal.log(f'{TAG} material={mat.get_name()} source={src.get_name()}:{out_name}')


if __name__ == '__main__':
    mpc = collection()
    for path in MATERIALS:
        m = unreal.load_asset(path)
        if m is None:
            raise RuntimeError('Missing ' + path)
        apply(m, mpc)
    unreal.log(f'{TAG}_READY materials={len(MATERIALS)}')
