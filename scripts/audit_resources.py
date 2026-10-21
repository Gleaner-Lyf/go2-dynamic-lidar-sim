#!/usr/bin/env python3
"""Check the installed dynamic scene's transitive local model references."""
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

from ament_index_python.packages import get_package_share_directory

files = set()
def visit(path):
    path = Path(path).resolve()
    if path in files:
        return
    if not path.is_file():
        raise FileNotFoundError(path)
    files.add(path)
    if path.suffix.lower() == '.obj':
        for line in path.read_text().splitlines():
            if line.startswith('mtllib '):
                visit(path.parent / line.split(None, 1)[1])
    elif path.suffix.lower() == '.mtl':
        for line in path.read_text().splitlines():
            if line.startswith(('map_', 'bump ', 'disp ')):
                visit(path.parent / line.split()[-1])
    elif path.suffix.lower() == '.dae':
        root = ET.parse(path).getroot()
        for image in root.findall('.//{*}library_images/{*}image/{*}init_from'):
            value = (image.text or '').strip()
            visit(Path(value[7:]) if value.startswith('file://') else path.parent / value)

config = Path(get_package_share_directory('go2_config'))
description = Path(get_package_share_directory('go2_description'))
world = config / 'worlds/dynamic_obstacles_p1.sdf'
root = ET.parse(world).getroot()
visit(world)
for uri in root.findall('.//uri'):
    value = (uri.text or '').strip()
    if '://' in value:
        raise ValueError('Dynamic world must resolve local resources: ' + value)
    visit(world.parent / value)
for plugin in root.findall('.//plugin'):
    if '/home/' in plugin.get('filename', ''):
        raise ValueError('Machine-specific plugin path')
urdf = subprocess.check_output(['xacro', str(description / 'xacro/robot_mid360.xacro')], text=True)
robot = ET.fromstring(urdf)
for mesh in robot.findall('.//mesh'):
    value = mesh.attrib['filename']
    if not value.startswith('file://'):
        raise ValueError('Unexpected mesh URI: ' + value)
    visit(value[7:])
for suffix in ['mid360.xacro', 'robot_mid360.xacro', 'robot.xacro', 'gazebo.xacro', 'leg.xacro', 'const.xacro', 'materials.xacro', 'transmission.xacro']:
    visit(description / 'xacro' / suffix)
for path in [config / 'rviz/mid360.rviz', config / 'meshes/pedestrians/SOURCE_LICENSE.txt']:
    visit(path)
plugin_prefix = Path(subprocess.check_output(['ros2', 'pkg', 'prefix', 'champ_gazebo'], text=True).strip())
assert (plugin_prefix / 'lib/libPedestrianPathController.so').is_file(), 'Build pedestrian plugin first'
print(json.dumps({'status':'passed', 'checked_local_files':len(files),
    'obj_materials':'resolved', 'dae_images':'resolved', 'plugin':'installed',
    'files':[str(x) for x in sorted(files)]}, indent=2))
