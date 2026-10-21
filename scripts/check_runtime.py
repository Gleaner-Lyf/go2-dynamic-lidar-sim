#!/usr/bin/python3
"""Read-only check: point clouds, clock, joints, odometry and pedestrian poses."""
import argparse
import json
import math
import subprocess
import tempfile
import time
from pathlib import Path

import rclpy
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import PointCloud2, JointState
from nav_msgs.msg import Odometry
from rosgraph_msgs.msg import Clock

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--duration', type=float, default=15)
parser.add_argument('--output', type=Path)
args = parser.parse_args()
rclpy.init()
node = rclpy.create_node('go2_dynamic_scene_check')
clouds, joints, clocks, odoms = [], [], [], []
def cloud(msg):
    clouds.append({'wall':time.monotonic(), 'sim':msg.header.stamp.sec+msg.header.stamp.nanosec/1e9,
        'frame':msg.header.frame_id,'width':msg.width,'height':msg.height,
        'fields':[f.name for f in msg.fields]})
subscriptions = [node.create_subscription(PointCloud2,'/mid360/points',cloud,qos_profile_sensor_data),
    node.create_subscription(JointState,'/joint_states',lambda m:joints.append(len(m.name)),qos_profile_sensor_data),
    node.create_subscription(Clock,'/clock',lambda m:clocks.append(m.clock.sec+m.clock.nanosec/1e9),qos_profile_sensor_data),
    node.create_subscription(Odometry,'/odom',lambda m:odoms.append(m.header.frame_id),qos_profile_sensor_data)]
samples = {}
with tempfile.TemporaryFile(mode='w+') as poses:
    process = subprocess.Popen(['ign','topic','-e','-t','/world/dynamic_obstacles_p1/dynamic_pose/info','--json-output'],stdout=poses)
    started = time.monotonic()
    try:
        while time.monotonic()-started < args.duration:
            rclpy.spin_once(node,timeout_sec=0.1)
    finally:
        process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        poses.seek(0)
        raw=poses.read()
    decoder=json.JSONDecoder()
    while raw.strip():
        raw=raw.lstrip()
        try:
            data,end=decoder.raw_decode(raw)
        except json.JSONDecodeError:
            break  # Ignore a partial final transport message after terminating capture.
        raw=raw[end:]
        for p in data.get('pose',[]):
            name=p.get('name','')
            if name in ['pedestrian_p1','pedestrian_p2','pedestrian_p3','go2_robot']:
                pos=p.get('position',{})
                samples.setdefault(name,[]).append([float(pos.get(k,0)) for k in ['x','y','z']])
spans={}
for name,values in samples.items():
    spans[name]=math.hypot(max(p[0] for p in values)-min(p[0] for p in values),max(p[1] for p in values)-min(p[1] for p in values))
def rate(axis):
    return (len(clouds)-1)/(clouds[-1][axis]-clouds[0][axis]) if len(clouds)>1 and clouds[-1][axis]>clouds[0][axis] else None
report={'cloud_messages':len(clouds),'cloud_hz_wall':rate('wall'),'cloud_hz_sim':rate('sim'),
    'cloud_last':clouds[-1] if clouds else None,'joint_messages':len(joints),
    'joint_count':joints[-1] if joints else None,'odom_messages':len(odoms),
    'clock_advanced':len(clocks)>1 and clocks[-1]>clocks[0], 'xy_spans_m':spans}
report['passed']=len(clouds)>1 and bool(joints) and joints[-1]==12 and report['clock_advanced'] and all(spans.get(n,0)>0.1 for n in ['pedestrian_p1','pedestrian_p2','pedestrian_p3'])
print(json.dumps(report,indent=2))
if args.output:
    args.output.write_text(json.dumps(report,indent=2)+'\n')
node.destroy_node()
rclpy.shutdown()
raise SystemExit(0 if report['passed'] else 1)
