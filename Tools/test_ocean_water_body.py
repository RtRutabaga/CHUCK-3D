"""Isolated editor-only ocean integration trial; never changes the playable map."""
import json
from pathlib import Path
import unreal
root=Path(__file__).resolve().parents[1]
world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
world.get_world_settings().set_editor_property('default_game_mode',unreal.GameModeBase)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
zone=actors.spawn_actor_from_class(unreal.WaterZone,unreal.Vector(0,0,-60))
ocean=actors.spawn_actor_from_class(unreal.WaterBodyOcean,unreal.Vector(0,0,-60))
component=ocean.get_component_by_class(unreal.WaterBodyOceanComponent)
component.set_editor_property('affects_landscape',False)
component.set_editor_property('water_material',unreal.load_asset('/Water/Materials/WaterSurface/Water_Material_Ocean'))
component.set_editor_property('water_zone_override',zone)
report={'engine':'5.7.4','editor_only':True,'rendered':False,'ocean_class':ocean.get_class().get_name(),
        'material':str(component.get_water_material()),'spline_points':ocean.get_component_by_class(unreal.WaterSplineComponent).get_number_of_spline_points(),
        'ocean_extent':str(component.get_editor_property('ocean_extents')),
        'zone':str(component.get_editor_property('water_zone_override')),'landscape_deformation':False,
        'project_plugin_config_changed':False,
        'decision':'Use shared Water-plugin textures in an independent harbor material on existing cut sea meshes. Ocean zone/spline data and continuous surface do not directly preserve the two basement/sewer shaft exclusions in the runtime-built docks.'}
if not unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Review/WaterOceanTrial'): raise RuntimeError('Trial map save failed')
(root/'Local/ocean-water-trial.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('CHUCK_OCEAN_TRIAL '+json.dumps(report))
