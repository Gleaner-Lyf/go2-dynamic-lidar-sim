> 本地整理版本：源码与验证已完成；GitHub 尚未登录，仓库尚未创建/推送。下列账号自解析克隆命令供登录并建仓后使用。

# 中文复现手册 A–N

仓库：（尚待 GitHub 登录后创建私有仓库并写入真实 URL） 。本手册针对 Ubuntu 22.04 + ROS2 Humble + Ignition Gazebo Fortress；私有仓库要求当前 GitHub 账号有读取权限。终端 A 用于编译和持续运行仿真，终端 B 用于键盘，终端 C 用于只读检查。命令均使用 Bash。

## A. 系统与版本检查（终端 A，任意目录）

```bash
cat /etc/os-release
source /opt/ros/humble/setup.bash
echo "$ROS_DISTRO"
ign gazebo --versions
command -v ros2
```

预期 Ubuntu 22.04、humble、Gazebo 6.x；本机已验证 6.18.0。若找不到 setup.bash 或 ign，先按 [ROS Humble 官方 Ubuntu 安装文档](https://docs.ros.org/en/humble/Installation/Ubuntu-Install-Debians.html) 与 [Fortress 官方安装文档](https://gazebosim.org/docs/fortress/install_ubuntu/) 准备平台，再继续。不要 source Foxy、Jazzy 或 Classic 工作空间；新开终端后只加载本手册指定环境。本文不要求重装 ROS 或显卡驱动，也不宣称精确锁定另一台电脑的全部系统包版本。

## B. 依赖准备（终端 A，任意目录）

下列 sudo 命令安装构建工具、Fortress 开发库、ROS 桥接、控制器、定位、RViz 和键盘节点。需要已配置可用的 Ubuntu / ROS Humble 软件源。

```bash
sudo apt update
sudo apt install git gh build-essential cmake python3-colcon-common-extensions \
  python3-rosdep python3-vcstool libignition-gazebo6-dev \
  libignition-plugin-dev libignition-sensors6-dev \
  libignition-transport11-dev libignition-msgs8-dev \
  ros-humble-ros-gz ros-humble-gz-ros2-control \
  ros-humble-ros2-control ros-humble-ros2-controllers \
  ros-humble-robot-localization ros-humble-diagnostic-updater \
  ros-humble-rviz2 ros-humble-teleop-twist-keyboard
```

预期 apt 正常完成；若出现找不到软件包，先检查软件源、`apt-cache policy` 和 A 中的系统版本，不要换成其他发行版的包。`libignition-plugin-dev` 是本机实际存在的开发包；固定 Velodyne 子模块 manifest 中的 `libignition-plugin1-dev` 不是本机可用的 apt 包名，后面的 rosdep 命令因此显式跳过它，并已在此安装正确开发包。其余显式跳过项是上游使用的 apt 名称与 rosdep 键不一致。

### diagnostic_updater 的标准解决方式

原机安装的 4.0.6 没有 `libdiagnostic_updater.so`，但当前 `robot_localization/ekf_node` 链接它。旧启动曾把解压的 4.0.7 动态库塞进日志目录并设置 LD_LIBRARY_PATH。本项目不上传、不查找那个目录。

优先使用 apt 提供的 `ros-humble-diagnostic-updater >=4.0.7`。如果可用软件源仍只有旧版本，D 中会导入官方 4.0.7 的固定源码并通过正常 colcon overlay 构建；共享库安装到新工作空间的标准 install 路径，source 后由 ROS 环境加载。这条源码路线已在干净检出中实际验证。不要手工复制 .so，也不要把原日志目录加进新终端。

## C–E. 克隆、固定依赖与工作空间（终端 A）

```bash
mkdir -p ~/go2_dynamic_ws/src
cd ~/go2_dynamic_ws/src
gh auth login --hostname github.com --git-protocol https --web
gh auth setup-git
gh repo clone "$(gh api user --jq .login)/go2-dynamic-lidar-sim" go2-dynamic-lidar-sim -- --recurse-submodules
cd ~/go2_dynamic_ws
git -C src/go2-dynamic-lidar-sim submodule status

if ! dpkg --compare-versions "$(dpkg-query -W -f='${Version}' ros-humble-diagnostic-updater)" ge 4.0.7; then
  vcs import src < src/go2-dynamic-lidar-sim/dependencies.repos
fi
```

预期 Velodyne 子模块固定为 `073b13d17f7478176492fe367ead10515353d917`；前缀不能是 `-`，表示尚未获取。若上一步漏了递归克隆，运行：

```bash
git -C ~/go2_dynamic_ws/src/go2-dynamic-lidar-sim submodule update --init --recursive
```

首次克隆私有仓库时，使用有读取权限的 GitHub 账号登录（例如 `gh auth login`），不要把令牌写进 clone URL。源码备选依赖固定在 `a1b95037eeab10592a8268be2a67cadaad6673da`，不是滚动分支。若希望完整复现本次已验证的源码依赖路线，即使 apt 版本足够，也可在工作空间根目录显式运行一次 `vcs import src < src/go2-dynamic-lidar-sim/dependencies.repos`。

项目不使用 LFS；必须获取 `.gitmodules` 子模块，不需要 LFS 安装或购买服务。所有本目标场景的模型、材质和基础几何体随仓库提供。

## F. rosdep 安装剩余依赖（终端 A，工作空间根目录）

```bash
cd ~/go2_dynamic_ws
source /opt/ros/humble/setup.bash
if [ ! -f /etc/ros/rosdep/sources.list.d/20-default.list ]; then
  sudo rosdep init
fi
rosdep update
mapfile -t GO2_PACKAGE_PATHS < <(colcon list --packages-up-to go2_config champ_teleop --paths-only)
printf '%s\n' "${GO2_PACKAGE_PATHS[@]}"
rosdep install --from-paths "${GO2_PACKAGE_PATHS[@]}" --ignore-src --rosdistro humble -y \
  --skip-keys 'libignition-gazebo6-dev libignition-plugin1-dev libignition-sensors6-dev libignition-transport11-dev libignition-msgs8-dev'
```

预期显示本目标场景所需包目录，并成功安装依赖；启用源码备选时包括 diagnostic_updater。只选择目标包及递归依赖，避免把上游导航示例的整套额外依赖当成必需。上述 skip-keys 对应 B 中已明确安装的 Fortress 开发库；不是忽略实际缺失依赖。网络失败应先恢复软件源/rosdep 访问，再重跑；不要隐藏错误后继续编译。

## G. 编译（终端 A，工作空间根目录）

```bash
cd ~/go2_dynamic_ws
colcon build --packages-up-to go2_config champ_teleop \
  --allow-overriding diagnostic_updater --symlink-install --parallel-workers 2 \
  --cmake-args -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

预期 Summary 中目标包全部 finished，没有 failed/aborted。源码备选路线共有 12 个目标包。`--allow-overriding` 明确允许用官方 4.0.7 源码覆盖系统旧诊断包；没有该源码包时该选项不影响其他包。内存不足可将 `--parallel-workers 2` 改成 1。构建失败先查看 `log/latest_build/具体包/stderr.log`；`build/install/log` 都是本机产物，不应上传。

## H. 加载并检查（终端 A，工作空间根目录）

```bash
source /opt/ros/humble/setup.bash
source ~/go2_dynamic_ws/install/setup.bash
cd ~/go2_dynamic_ws
./src/go2-dynamic-lidar-sim/scripts/check_environment.sh
./src/go2-dynamic-lidar-sim/scripts/audit_resources.py
```

预期环境检查通过，诊断库版本至少 4.0.7，EKF 的 `ldd` 无 not found；资源检查输出 passed，OBJ/MTL、DAE、插件和场景可解析。脚本从 ROS 包前缀定位资源，不依赖原机器用户名或日志。

若提示包不存在，检查是否编译成功、是否 source 当前工作空间，以及 `ros2 pkg prefix go2_config` 指向哪里。若 Python 找不到 ROS 模块，使用干净终端并避免 Conda 抢占 Python；项目检查脚本指定 Ubuntu 的 `/usr/bin/python3`。

## I. 启动完整场景（终端 A，保持运行）

```bash
cd ~/go2_dynamic_ws
./src/go2-dynamic-lidar-sim/scripts/start_sim.sh
```

预期 Gazebo 显示地面、边界和内部墙、Go2、三个移动行人；RViz 显示机器人和场景点云。launch 默认 world 已设置为随 go2_config 安装的 `dynamic_obstacles_p1.sdf`。

原机因为图形驱动初始化失败/白屏/第一帧停滞，使用 CPU 软件渲染成功。若本机遇到同类问题，在终端 A Ctrl+C 退出本次启动，再执行：

```bash
./src/go2-dynamic-lidar-sim/scripts/start_sim.sh --software-rendering
```

不要同时重复启动两个实例。软件渲染会增加 CPU 开销并可能降低现实时间推进速度；不是每台电脑都需要。脚本等价于设置可选 `LIBGL_ALWAYS_SOFTWARE=1` 后执行：

```bash
ros2 launch go2_config gazebo_mid360.launch.py \
  world_init_z:=0.275 mid360_bridge:=true rviz:=true gui:=true headless:=False
```

库路径由 launch 根据 ament 前缀设置。基础几何尺寸、行人参数和雷达数值配置未在可移植性整理中改变。

## J. RViz 检查（终端 C，仿真持续运行）

```bash
source /opt/ros/humble/setup.bash
source ~/go2_dynamic_ws/install/setup.bash
ros2 topic info /mid360/points --verbose
ros2 topic echo /mid360/points --once --field header
```

预期发布节点为 `mid360_points_bridge`，类型 PointCloud2，frame=`mid360_link`。RViz 固定坐标系 `odom`，PointCloud2 话题 `/mid360/points`。默认已经打开 RViz；若只关闭了该窗口，可以在终端 C 重新打开：

```bash
rviz2 -d "$(ros2 pkg prefix go2_config)/share/go2_config/rviz/mid360.rviz"
```

若有话题无消息，继续检查下面的原生传感器话题与仿真时钟。若点云到达但 RViz 红色错误，检查 Fixed Frame 和 TF，而不是仅修改 QoS。

## K. 键盘控制（终端 B，保持运行）

```bash
source /opt/ros/humble/setup.bash
source ~/go2_dynamic_ws/install/setup.bash
ros2 run teleop_twist_keyboard teleop_twist_keyboard \
  --ros-args -p speed:=0.12 -p turn:=0.3
```

输入焦点留在终端 B，切换英文输入法。i 前进、逗号后退、j/l 左右转、u/o 边前进边转、k 或空格停止。当前节点松开按键不会自动停止；CHAMP 保存最后速度，没有自动指令超时。在继续操作其他终端前按 k。不要同时运行其他 `/cmd_vel` 发布者。

终端 A 的仿真必须继续运行；终端 B 的键盘也应保持运行到主动停止并退出。原地转向可能有平移，应观察墙体与行人位置。

## L. 只读验证（终端 C）

```bash
source /opt/ros/humble/setup.bash
source ~/go2_dynamic_ws/install/setup.bash
cd ~/go2_dynamic_ws
ros2 control list_controllers
ros2 topic list
ros2 topic hz /mid360/points
```

两个控制器预期均为 active；hz 显示接收统计后用 Ctrl+C 结束这个统计命令。它统计现实时间接收频率，与仿真配置的 10 Hz 不一定相同。不得仅凭这个差异判定丢帧。可运行只读综合检查，同时输出现实时间和消息时间戳统计：

```bash
./src/go2-dynamic-lidar-sim/scripts/check_runtime.py --duration 15
ign topic -l
ign topic -e -n 1 -t /world/dynamic_obstacles_p1/dynamic_pose/info --json-output
```

预期综合检查 passed=true、12 关节、clock_advanced=true，P1/P2/P3 的 xy_spans_m 大于 0.1 m。该脚本不会发送速度或移动机器人。最后一条输出实际 Gazebo 位姿，可重复采样确认行人实际移动；不是只看目标路径或命令。

点云不动/不更新时依次检查仿真是否暂停、`/clock` 是否推进、Ignition 是否有 `/mid360/points`、桥接节点是否存在，再检查 ROS 消息。plugin 加载失败时检查 `ros2 pkg prefix champ_gazebo` 下的 lib/libPedestrianPathController.so；不要把原电脑绝对路径写回 SDF。

## M. 正常停止与退出

1. 终端 B 按 k 或空格，观察 Go2 稳定；随后 Ctrl+C 退出键盘。
2. 如需在终端 C 明确重发停止，可执行下列脚本；它连续发送十次零速度。

```bash
cd ~/go2_dynamic_ws
./src/go2-dynamic-lidar-sim/scripts/stop_robot.sh
```

3. 终端 A 按 Ctrl+C，等待 launch 关闭 Gazebo、RViz 和节点。不要只关闭 Gazebo/RViz 图形窗口而让启动终端及控制节点残留。

## N. 下次打开的最简步骤

已经编译并安装依赖后，不需要重新 clone 或编译。终端 A：

```bash
source /opt/ros/humble/setup.bash
source ~/go2_dynamic_ws/install/setup.bash
cd ~/go2_dynamic_ws
./src/go2-dynamic-lidar-sim/scripts/start_sim.sh
```

需要软件渲染时加 `--software-rendering`。终端 B 重复 K；停止重复 M。源码改动后再重新编译相应包。

## 常见故障

| 现象 | 检查与处理 |
|---|---|
| 私有 clone 权限失败 | 确认已登录有读取权限的 GitHub 账号，不在文档或 URL 中写令牌。 |
| libdiagnostic_updater.so not found | 按 B/D 升级标准包或导入固定官方源码，重新编译和 source；运行环境脚本复查。 |
| 缺 champ_config | 按 G 构建完整目标依赖并 source；不要只单独构建 go2_config。 |
| CMake 找不到 ignition 库 | 核对 B 的开发包以及所加载的 Humble 环境；不安装 Classic 替代。 |
| 控制器 inactive | 等待初始化，检查启动日志中的 controller_manager / 加载失败原因；不要在 inactive 时测试行走。 |
| 模型或材质缺失 | 运行 audit_resources.py，核对 clone 子模块和 package 安装规则；查看 SOURCE_LICENSE 与引用闭包。 |
| 键盘不响应 | 检查终端焦点、英文输入、`ros2 topic info /cmd_vel --verbose`、控制器 active；先按 k 再检查。 |
| 窗口白屏或仿真停在第一帧 | 结束此次 launch，尝试软件渲染选项；保留实际错误信息，不擅自重装驱动。 |
| 软件渲染慢 | 记录现实时间与仿真时间、负载和渲染器；在支持的机器上尝试正常渲染，当前仓库未保证固定接收速率。 |

本项目只承诺文档覆盖的目标入口。上游遗留的导航、其他示例世界和旧 ROS1 launch 不属于此次运行验证范围。
