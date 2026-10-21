#!/usr/bin/env bash
set -e
[[ "${ROS_DISTRO:-}" == humble ]] || { echo '需要 ROS2 Humble 环境。' >&2; exit 1; }
version="$(ign gazebo --versions)"
[[ "$version" == 6.* ]] || { echo "需要 Fortress / Gazebo 6，检测到：$version" >&2; exit 1; }
ros2 pkg prefix go2_config
ros2 pkg prefix champ_gazebo
diagnostic_version="$(/usr/bin/python3 -c 'from ament_index_python.packages import get_package_share_directory; import xml.etree.ElementTree as E; from pathlib import Path; print(E.parse(Path(get_package_share_directory("diagnostic_updater"))/"package.xml").getroot().findtext("version"))')"
dpkg --compare-versions "$diagnostic_version" ge 4.0.7 || {
  echo "diagnostic_updater=$diagnostic_version，需要 >=4.0.7；参阅 docs/REPRODUCE_zh.md 的依赖步骤。" >&2
  exit 1
}
ekf="$(ros2 pkg prefix robot_localization)/lib/robot_localization/ekf_node"
missing="$(ldd "$ekf" | sed -n '/not found/p')"
[[ -z "$missing" ]] || { echo "$missing" >&2; exit 1; }
echo "环境检查通过：ROS Humble，Gazebo $version，diagnostic_updater $diagnostic_version。"
