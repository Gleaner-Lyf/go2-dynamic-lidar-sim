#!/usr/bin/env bash
set -e
repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ "${1:-}" == "--help" ]]; then
  echo "Usage: $0 [--software-rendering] [ros2 launch name:=value ...]"
  echo 'Source the built workspace first. Software rendering is optional.'
  exit 0
fi
if [[ "${1:-}" == "--software-rendering" ]]; then
  export LIBGL_ALWAYS_SOFTWARE=1
  shift
fi
if [[ "${ROS_DISTRO:-}" != humble ]]; then
  echo '请先 source /opt/ros/humble/setup.bash 和本工作空间的 install/setup.bash。' >&2
  exit 1
fi
"$repo_dir/scripts/check_environment.sh"
exec ros2 launch go2_config gazebo_mid360.launch.py \
  world_init_z:=0.275 mid360_bridge:=true rviz:=true gui:=true headless:=False "$@"
