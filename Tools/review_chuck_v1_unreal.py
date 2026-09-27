"""Render isolated editor-only v1 poses. Does not save a map or change gameplay."""
from pathlib import Path
import time
import traceback
import unreal

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Local/V1UnrealReview'
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
hero.skeletal_mesh_component.set_visibility(False)
subobjects=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
parent=subobjects.k2_gather_subobject_data_for_instance(hero)[0]
params=unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.PoseableMeshComponent)
added,reason=subobjects.add_new_subobject(params)
data=unreal.SubobjectDataBlueprintFunctionLibrary.get_data(added)
component=unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(data)
if not isinstance(component,unreal.PoseableMeshComponent):
    raise RuntimeError('Cannot create review pose component: '+str(reason))
component.set_skinned_asset_and_update(unreal.load_asset(DEST+'/SK_Chuck'))
pose_options=unreal.AnimPoseEvaluationOptions()
pose_options.set_editor_property('evaluation_type',unreal.AnimDataEvalType.RAW)
pose_options.set_editor_property('should_retarget',False)
camera=actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(125,-135,78))
camera.camera_component.set_field_of_view(38)
camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),unreal.Vector(0,0,32)),False)
poses=[('front','Idle',0,(165,0,66)),('three_quarter','Idle',0,(125,-135,78)),
       ('walk_side','WalkLoop',.1,(0,-185,55)),('land_side','JumpLand',.1,(0,-185,55))]
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
        name,clip,t,eye=poses[index]
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(unreal.load_asset(DEST+'/Animations/AS_Chuck_'+clip),t,pose_options)
        for bone in unreal.AnimPoseExtensions.get_bone_names(pose):
            transform=unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.WORLD)
            component.set_bone_transform_by_name(bone,transform,unreal.BoneSpaces.COMPONENT_SPACE)
        if not unreal.ChuckReviewLibrary.refresh_editor_pose(component):
            raise RuntimeError('Editor pose refresh unavailable; rebuild Chuck3DEditor')
        for bone in ('pelvis','foot_L','foot_R','head'):
            expected=unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.WORLD).translation
            actual=component.get_bone_transform_by_name(bone,unreal.BoneSpaces.COMPONENT_SPACE).translation
            if (actual-expected).length()>.001:
                raise RuntimeError('Review pose mismatch: '+name+' '+bone)
        unreal.log('CHUCK_V1_REVIEW_POSE '+name+' '+str(component.get_bone_transform_by_name('pelvis',unreal.BoneSpaces.COMPONENT_SPACE)))
        camera.set_actor_location(unreal.Vector(*eye),False,False)
        camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),unreal.Vector(0,0,32)),False)
        task=unreal.AutomationLibrary.take_high_res_screenshot(1280,960,str(OUT/(name+'.png')),camera=camera,delay=1.0)
        next_at=time.monotonic()+2
    except Exception:
        unreal.log_error(traceback.format_exc())
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    finally:
        busy=False


handle=unreal.register_slate_post_tick_callback(tick)
