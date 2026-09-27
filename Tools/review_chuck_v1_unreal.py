"""Render isolated editor-only v1 poses. Does not save a map or change gameplay."""
from pathlib import Path
import time
import traceback
import unreal

ROOT=Path(__file__).resolve().parents[1]
GROOM='-ChuckGroomReview' in unreal.SystemLibrary.get_command_line()
OUT=ROOT/('Local/GroomUnrealReview' if GROOM else 'Local/V1UnrealReview')
OUT.mkdir(parents=True,exist_ok=True)
DEST='/Game/Characters/Chuck/V1'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
# Operate in a new unsaved editor world; never save over the docks map.
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).new_level('/Temp/ChuckV1Review')
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
ground=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,-3))
ground.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
ground.set_actor_scale3d(unreal.Vector(8,8,.06))
ground.static_mesh_component.set_material(0,unreal.load_asset('/Game/Art/Materials/M_Stone'))
sun=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,250),unreal.Rotator(-35,-35,0))
sun.light_component.set_editor_property('intensity',5.0)
sun.light_component.set_editor_property('light_color',unreal.Color(255,235,211,255))
actors.spawn_actor_from_class(unreal.SkyAtmosphere,unreal.Vector())
sky=actors.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,100))
sky.light_component.set_editor_property('intensity',1.0)
sky.light_component.set_editor_property('real_time_capture',True)
fill=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,200),unreal.Rotator(-25,140,0))
fill.light_component.set_editor_property('intensity',2.0)
fill.light_component.set_editor_property('cast_shadows',False)
hero=actors.spawn_actor_from_class(unreal.SkeletalMeshActor,unreal.Vector(0,0,0))
component=hero.skeletal_mesh_component
component.set_skeletal_mesh_asset(unreal.load_asset(DEST+('/SK_Chuck_Groomed' if GROOM else '/SK_Chuck')))
subobjects=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
parent=subobjects.k2_gather_subobject_data_for_instance(hero)[0]
if GROOM:
    for group in ('Fur_Body','Fur_Back','Fur_Cream'):
        params=unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.GroomComponent)
        added,reason=subobjects.add_new_subobject(params)
        data=unreal.SubobjectDataBlueprintFunctionLibrary.get_data(added)
        hair=unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(data)
        assert isinstance(hair,unreal.GroomComponent),str(reason)
        hair.attach_to_component(component,'',unreal.AttachmentRule.KEEP_RELATIVE,
            unreal.AttachmentRule.KEEP_RELATIVE,unreal.AttachmentRule.KEEP_RELATIVE,False)
        hair.set_groom_asset(unreal.load_asset(DEST+'/GR_Chuck_'+group))
        hair.set_binding_asset(unreal.load_asset(DEST+'/GB_Chuck_'+group))
        simulation=hair.get_editor_property('simulation_settings')
        simulation.set_editor_property('override_settings',True)
        hair.set_editor_property('simulation_settings',simulation)
        hair.set_enable_simulation(False)
        hair.set_material(0,unreal.load_asset(DEST+'/M_'+group))
# Cigarette prop and upright smoke on socket_cigarette, oriented like the
# runtime (AChuckCharacter::BeginPlay): the prop's +X along the exported bones'
# local axis, read from the imported rest pose (thigh -> knee).
ref=unreal.AnimPoseExtensions.get_reference_pose(component.skeletal_mesh_asset.skeleton)
thigh=unreal.AnimPoseExtensions.get_bone_pose(ref,'thigh_L',unreal.AnimPoseSpaces.WORLD)
knee=unreal.AnimPoseExtensions.get_bone_pose(ref,'calf_L',unreal.AnimPoseSpaces.WORLD)
bone_axis=unreal.MathLibrary.inverse_transform_direction(thigh,knee.translation-thigh.translation)
def add_static(parent,socket,mesh):
    params=unreal.AddNewSubobjectParams(parent_handle=subobjects.k2_gather_subobject_data_for_instance(hero)[0],new_class=unreal.StaticMeshComponent)
    added,reason=subobjects.add_new_subobject(params)
    comp=unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(added))
    assert isinstance(comp,unreal.StaticMeshComponent),str(reason)
    comp.set_static_mesh(unreal.load_asset(DEST+'/Cigarette/'+mesh))
    comp.attach_to_component(parent,socket,unreal.AttachmentRule.SNAP_TO_TARGET,
        unreal.AttachmentRule.SNAP_TO_TARGET,unreal.AttachmentRule.KEEP_RELATIVE,False)
    return comp
cigarette=add_static(component,'socket_cigarette','SM_Cigarette')
cigarette.set_relative_rotation(unreal.MathLibrary.make_rot_from_x(bone_axis),False,False)
smoke=add_static(cigarette,'','SM_CigaretteSmoke')
smoke.set_relative_location(unreal.Vector(cigarette.get_editor_property('static_mesh').get_bounding_box().max.x,0,0),False,False)
smoke.set_absolute(False,True,False)
smoke.set_world_rotation(unreal.Rotator(0,0,0),False,False)
pose_options=unreal.AnimPoseEvaluationOptions()
pose_options.set_editor_property('evaluation_type',unreal.AnimDataEvalType.RAW)
pose_options.set_editor_property('should_retarget',False)
camera=actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(125,-135,78))
camera.camera_component.set_field_of_view(38)
camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),unreal.Vector(0,0,32)),False)
poses=[('front','Idle',0,(165,0,66)),('three_quarter','Idle',0,(125,-135,78)),
       ('face','Idle',0,(52,-46,62),(9,-1,53)),
       ('walk_side','WalkLoop',.1,(0,-185,55)),('land_side','JumpLand',.1,(0,-185,55))]
if GROOM:
    poses.extend([('rat_height','WalkLoop',.1,(-220,0,65)),('elevated','WalkLoop',.1,(-268,0,329))])
index=0
task=None
busy=False
next_at=time.monotonic()+8
deadline=time.monotonic()+120
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def tick(dt):
    global index,task,next_at,busy
    # Screenshot setup can pump Slate recursively before returning its task.
    if busy:
        return
    busy=True
    try:
        if time.monotonic()>deadline:
            raise RuntimeError('V1 review timed out')
        if time.monotonic()<next_at:
            return
        if task is not None:
            if not task.is_task_done():
                return
            task=None
            index+=1
        if index==len(poses):
            unreal.log('CHUCK_V1_REVIEW_READY '+str(OUT))
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        name,clip,t,eye=poses[index][:4]
        target=poses[index][4] if len(poses[index])>4 else (0,0,32)
        anim=unreal.load_asset(DEST+'/Animations/AS_Chuck_'+clip)
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(anim,t,pose_options)
        # Apply the evaluated clip pose directly (the single-node anim path does
        # not evaluate reliably in a non-ticking editor world).
        bones=list(unreal.AnimPoseExtensions.get_bone_names(pose))
        transforms=[unreal.AnimPoseExtensions.get_bone_pose(pose,b,unreal.AnimPoseSpaces.WORLD) for b in bones]
        if not unreal.ChuckReviewLibrary.set_editor_component_pose(component,bones,transforms):
            raise RuntimeError('Editor pose apply unavailable; rebuild Chuck3DEditor')
        for bone in ('pelvis','foot_L','foot_R','head'):
            expected=unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.WORLD).translation
            actual=component.get_socket_transform(bone,unreal.RelativeTransformSpace.RTS_COMPONENT).translation
            if (actual-expected).length()>.05:
                raise RuntimeError('Review pose mismatch: '+name+' '+bone+' expected='+str(expected)+' actual='+str(actual))
        unreal.log('CHUCK_V1_REVIEW_POSE '+name+' '+str(component.get_socket_transform('pelvis',unreal.RelativeTransformSpace.RTS_COMPONENT)))
        camera.camera_component.set_field_of_view({'rat_height':78,'elevated':65}.get(name,38))
        camera.set_actor_location(unreal.Vector(*eye),False,False)
        camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),unreal.Vector(*target)),False)
        task=unreal.AutomationLibrary.take_high_res_screenshot(1280,960,str(OUT/(name+'.png')),camera=camera,delay=1.0)
        next_at=time.monotonic()+2
    except Exception:
        unreal.log_error(traceback.format_exc())
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    finally:
        busy=False


handle=unreal.register_slate_post_tick_callback(tick)
