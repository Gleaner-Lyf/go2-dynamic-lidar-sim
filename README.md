> 本地整理版本：源码与验证已完成；GitHub 尚未登录，仓库尚未创建/推送。下列账号自解析克隆命令供登录并建仓后使用。

# Go2 三维动态行人场景与 MID-360 近似雷达仿真

基于 ROS2 Humble、Ignition Gazebo Fortress 和 CHAMP 的 Go2 仿真工程。场景包括地面、边界墙、内部墙、Go2、P1/P2/P3 三个动态行人，以及规则扫描的 MID-360 近似传感器。仓库地址：（尚待 GitHub 登录后创建私有仓库并写入真实 URL） 。本仓库为私有，克隆需要仓库访问权限。

已实测机器人前进、左右转向与停止，三个行人运动、两个关节控制器 active，以及 RViz 机器人和点云显示。当前实现不包含真实 MID-360 非重复扫描轨迹、逐点时间字段或 Unitree L1 仿真。

## 环境与验证范围

- 已验证：Ubuntu 22.04.5、ROS2 Humble、Ignition Gazebo 6.18.0 / Fortress，Python 3.10、GCC 11。
- 原工作空间实测：前进约 0.31 m；左转 43.7°、右转 76.8°；稳定后约 3 秒停止漂移 0.078 mm。转向伴随约 0.20～0.27 m 平移。
- 干净检出、路径与独立运行验证结果见 [验证记录](docs/VALIDATION_zh.md)。没有在另一台电脑或全新 Ubuntu 安装上实测。
- 软件渲染时，原机按现实时间统计的点云接收频率约 1.9～3.6 Hz，传感器配置频率为 10 Hz。频率比较受仿真推进速度、渲染和统计口径影响，未进一步测量前不能将其直接解释成丢帧。

## 快速复现

以下假设 Ubuntu 22.04 已配置 ROS2 Humble 软件源和可工作的 Humble / Fortress。完整步骤、依赖说明、预期结果、失败检查和正常退出见 [中文复现手册 A–N](docs/REPRODUCE_zh.md)。不要加载其他 ROS 发行版或 Gazebo Classic 工作空间。

终端 A：安装构建工具和本工程明确需要的系统包。sudo 仅用于安装依赖，不重装 ROS 或显卡驱动。

```bash
source /opt/ros/humble/setup.bash
sudo apt update
sudo apt install git gh build-essential cmake python3-colcon-common-extensions \
  python3-rosdep python3-vcstool libignition-gazebo6-dev \
  libignition-plugin-dev libignition-sensors6-dev \
  libignition-transport11-dev libignition-msgs8-dev \
  ros-humble-ros-gz ros-humble-gz-ros2-control \
  ros-humble-ros2-control ros-humble-ros2-controllers \
  ros-humble-robot-localization ros-humble-diagnostic-updater \
  ros-humble-rviz2 ros-humble-teleop-twist-keyboard

mkdir -p ~/go2_dynamic_ws/src
cd ~/go2_dynamic_ws/src
gh auth login --hostname github.com --web
gh auth setup-git
gh repo clone "$(gh api user --jq .login)/go2-dynamic-lidar-sim" go2-dynamic-lidar-sim -- --recurse-submodules
cd ~/go2_dynamic_ws

# 旧软件源仍提供 diagnostic_updater 4.0.6 时，获取已固定提交的官方源码。
if ! dpkg --compare-versions "$(dpkg-query -W -f='${Version}' ros-humble-diagnostic-updater)" ge 4.0.7; then
  vcs import src < src/go2-dynamic-lidar-sim/dependencies.repos
fi

# 新机器只需 sudo rosdep init 一次；已初始化时跳过该命令。
if [ ! -f /etc/ros/rosdep/sources.list.d/20-default.list ]; then
  sudo rosdep init
fi
rosdep update
mapfile -t GO2_PACKAGE_PATHS < <(colcon list --packages-up-to go2_config champ_teleop --paths-only)
rosdep install --from-paths "${GO2_PACKAGE_PATHS[@]}" --ignore-src --rosdistro humble -y \
  --skip-keys 'libignition-gazebo6-dev libignition-plugin1-dev libignition-sensors6-dev libignition-transport11-dev libignition-msgs8-dev'

colcon build --packages-up-to go2_config champ_teleop \
  --allow-overriding diagnostic_updater --symlink-install --parallel-workers 2 \
  --cmake-args -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo
source install/setup.bash
./src/go2-dynamic-lidar-sim/scripts/audit_resources.py
./src/go2-dynamic-lidar-sim/scripts/start_sim.sh
```

若出现原机曾遇到的驱动初始化失败、白屏或第一帧停滞，退出此次启动后改用：

```bash
./src/go2-dynamic-lidar-sim/scripts/start_sim.sh --software-rendering
```

此选项设置 `LIBGL_ALWAYS_SOFTWARE=1`，使用 CPU 软件渲染，可能明显降低仿真和点云的现实时间速率。其他机器优先使用正常渲染，无需默认启用它。完整启动等价于：

```bash
ros2 launch go2_config gazebo_mid360.launch.py \
  world_init_z:=0.275 mid360_bridge:=true rviz:=true gui:=true headless:=False
```

终端 B：仿真启动并保持终端 A 运行后，打开新终端控制机器人：

```bash
source /opt/ros/humble/setup.bash
source ~/go2_dynamic_ws/install/setup.bash
ros2 run teleop_twist_keyboard teleop_twist_keyboard \
  --ros-args -p speed:=0.12 -p turn:=0.3
```

输入焦点应在终端 B，使用英文输入法。`i` 前进、`,` 后退、`j/l` 左右转、`u/o` 边前进边转；**`k` 或空格停止**。当前标准键盘节点松开按键不会自动停止。按 `k` 后再 `Ctrl+C` 退出键盘；最后在终端 A `Ctrl+C` 关闭仿真。

## 目录和来源

保留上游多 ROS 包结构、Git 历史和固定版本 Velodyne 子模块；目标场景所需的自定义内容均在此仓库，无其他个人工作空间依赖。

| 目录 | 内容 |
|---|---|
| `champ/` | CHAMP 控制、状态估计、Gazebo 启动、完整行人插件源码 |
| `robots/descriptions/go2_description/` | Go2 URDF/Xacro、DAE、碰撞、惯性、关节、MID-360 模型 |
| `robots/configs/go2_config/` | 动态场景、P1 OBJ/MTL、launch、控制配置、RViz |
| `velodyne/` | 固定提交的上游子模块 |
| `champ_teleop/` | 上游遥控包 |
| `scripts/` | 启动、检查、停止脚本 |
| `docs/` | 复现、结构、验证、上游原 README |
| `dependencies.repos` | 旧系统诊断库的官方源码备选 |

上游是 [rahgirrafi/unitree-go2-ros2](https://github.com/rahgirrafi/unitree-go2-ros2) 的 `ignition` 分支，基线 `5580296`。自定义工作包括动态行人场景、基于力的行人路径插件、P1 静态人形、MID-360 近似模型及桥接、RViz 配置；本次发布整理加入可移植路径、依赖声明、检查脚本和中文文档，没有更改步态算法和雷达数值参数。

三维资源随 ROS 包安装，SDF 基础几何体完整保留；P1 OBJ→MTL、Go2 DAE 引用由资源检查脚本递归检查。单个最大文件约 10.7 MB，当前不使用 Git LFS。无需单独下载运行时模型。许可证和来源见 [第三方说明](THIRD_PARTY_NOTICES.md)，上游原始说明见 [UPSTREAM_README](docs/UPSTREAM_README.md)。上游其他示例世界不属于本项目已验证入口。

## 数据链路和限制

键盘 `/cmd_vel` → CHAMP → 关节轨迹控制 → Gazebo Go2；行人插件控制三个物理模型；Ogre2 `gpu_lidar` 对场景生成点云 → `ros_gz_bridge` → `/mid360/points` → RViz。详细图示、话题与坐标系见 [工程结构](docs/ARCHITECTURE_zh.md)。

MID-360 近似参数：水平 720 点 / 360°，垂直 40 层 / 约 −7°～52°，距离 0.10～40 m，距离分辨率 0.01 m，高斯噪声标准差 0.01 m，配置频率 10 Hz。安装于 `trunk` 的 `(0.10,0,0.09)` m；点云类型 `sensor_msgs/msg/PointCloud2`、frame=`mid360_link`，字段 x/y/z/intensity/ring，无逐点时间。

已知限制：转向伴随平移；软件渲染速度受 CPU 与并发负载影响；P1 没有骨骼步行动画；雷达为规则网格近似；通用 `/imu/data` 桥接没有对齐当前模型的原生 IMU 话题；导航、SLAM、真实硬件、其他 ROS/Gazebo 版本和其他电脑未验证。排查方法见复现手册。
