"""Validate real Unreal v1 rest poses; restore the loops' closing sample interval."""
from pathlib import Path
import json
import math
import unreal

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'SourceAssets/Chuck/V1'
DEST='/Game/Characters/Chuck/V1'
table=json.loads((SOURCE.parent/'rig_proposal.json').read_text())
manifest=json.loads((SOURCE/'Animations/manifest.json').read_text())
mesh=unreal.load_asset(DEST+'/SK_Chuck')
skeleton=mesh.get_editor_property('skeleton')
EXT=unreal.AnimPoseExtensions
ref=EXT.get_reference_pose(skeleton)
names=[str(n) for n in EXT.get_bone_names(ref)]
assert set(names)==set(table)|{'SK_Chuck_Rig'}, names
errors=[]
for name,bone in table.items():
    pose=EXT.get_bone_pose(ref,name,unreal.AnimPoseSpaces.WORLD)
    expected=(bone['head'][0],-bone['head'][1],bone['head'][2])
    errors.append(math.dist((pose.translation.x,pose.translation.y,pose.translation.z),expected))
assert max(errors)<.01, max(errors)
container=EXT.get_bone_pose(ref,'SK_Chuck_Rig',unreal.AnimPoseSpaces.WORLD)
assert container.translation.length()<.01 and max(abs(v-1) for v in (container.scale3d.x,container.scale3d.y,container.scale3d.z))<.001
options=unreal.AnimPoseEvaluationOptions()
options.set_editor_property('evaluation_type',unreal.AnimDataEvalType.RAW)
options.set_editor_property('should_retarget',False)
options.set_editor_property('optional_skeletal_mesh',mesh)
report={'bone_count':len(names),'max_rest_head_error_cm':max(errors),'clips':[]}
for clip in manifest['clips']:
    anim=unreal.load_asset(DEST+'/Animations/AS_Chuck_'+clip['name'])
    length=anim.get_editor_property('sequence_length')
    if clip['loop'] and abs(length-clip['duration_s'])>.001:
        # Source loops intentionally omit a duplicate final sample. Unreal
        # needs it to interpolate across the closing 1/30-second interval.
        expected=(clip['frame_count']-1)/clip['fps']
        assert abs(length-expected)<.001, (clip['name'],length)
        model=anim.get_editor_property('data_model_interface')
        tracks=[str(n) for n in model.get_bone_track_names()]
        data={name:[] for name in tracks}
        for frame in range(clip['frame_count']):
            pose=EXT.get_anim_pose_at_time(anim,frame/clip['fps'],options)
            for name in tracks:
                data[name].append(EXT.get_bone_pose(pose,name,unreal.AnimPoseSpaces.LOCAL))
        controller=anim.get_editor_property('controller')
        controller.open_bracket('Restore closing loop interval',False)
        controller.set_number_of_frames(unreal.FrameNumber(clip['frame_count']),False)
        for name,keys in data.items():
            keys.append(keys[0])
            assert controller.set_bone_track_keys(name,[k.translation for k in keys],
                [k.rotation for k in keys],[k.scale3d for k in keys],False), name
        controller.close_bracket(False)
        unreal.EditorAssetLibrary.save_loaded_asset(anim,only_if_is_dirty=False)
    length=anim.get_editor_property('sequence_length')
    assert abs(length-clip['duration_s'])<.001,(clip['name'],length)
    # Imported animation roots must stay still, independently of source QA.
    for t in (0,length/2,length):
        pose=EXT.get_anim_pose_at_time(anim,t,options)
        root=EXT.get_bone_pose(pose,'root',unreal.AnimPoseSpaces.WORLD)
        assert root.translation.length()<.01,(clip['name'],t,root)
    max_delta=0
    for t in (length*.25,length*.5,length*.75):
        pose=EXT.get_anim_pose_at_time(anim,t,options)
        for name in ('pelvis','foot_L','foot_R','head'):
            current=EXT.get_bone_pose(pose,name,unreal.AnimPoseSpaces.WORLD).translation
            neutral=EXT.get_bone_pose(ref,name,unreal.AnimPoseSpaces.WORLD).translation
            max_delta=max(max_delta,(current-neutral).length())
    assert max_delta>.01,(clip['name'],'no imported pose motion')
    report['clips'].append({'name':clip['name'],'length_s':length,'root_static':True,'sampled_motion_cm':max_delta})
(ROOT/'Local/v1-import-validation.json').write_text(json.dumps(report,indent=2))
unreal.log('CHUCK_V1_IMPORT_VALIDATED '+json.dumps(report))
