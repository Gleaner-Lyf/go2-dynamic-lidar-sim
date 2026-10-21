# 验证记录与边界

日期：2026-10-21。不要将下列不同运行条件的结果合并成固定性能承诺。

## 原工作空间在当前机器的实测

Ubuntu 22.04.5、ROS2 Humble、Ignition Gazebo Fortress 6.18.0，Gazebo 与 RViz 双窗口，CPU 软件渲染。

| 检查 | 真实结果 |
|---|---|
| 机器人前进 | cmd linear.x=0.12 m/s；实际前进 0.309 m |
| 左转 | cmd angular.z=0.3 rad/s；实际 +43.655°，平移 0.195 m |
| 右转 | cmd angular.z=-0.3 rad/s；实际 -76.751°，平移 0.269 m |
| 最后停止 | 发零速度并等待稳定后，2.93 秒现实时间 / 2.712 秒仿真时间内漂移 0.078 mm |
| 行人 | P1/P2/P3 100 次实际位姿采样均出现位移 |
| 控制器 | joint_states_controller 与 joint_group_effort_controller 均 active |
| 关节 / 里程计 | 12 关节消息与 /odom 持续发布 |
| GUI | Gazebo 场景与 RViz 机器人、场景点云可见 |
| 点云 | PointCloud2，720×40，frame mid360_link，x/y/z/intensity/ring；无逐点时间字段 |
| 接收速率 | 静止检查约 3.60 Hz；运动检查约 1.90 Hz，均按现实时间统计 |

机器人运动证据取自 Gazebo 的模型实际位姿，而不是速度目标或估计里程计。各阶段持续时间和仿真推进不同，因此左右转角不是对称性测试。

## 独立干净检出构建

在原工作空间之外，从新仓库已提交的内容克隆，获取固定 Velodyne 子模块，并引入官方 diagnostic_updater 4.0.7 固定源码。

- 使用 `env -i` 清除环境，仅加载 `/opt/ros/humble/setup.bash`，通过 colcon 创建新 build/install。
- 目标为 go2_config、champ_teleop 及递归依赖；最终 12 个包构建成功。
- 构建系统/环境检查没有引用原工作空间 install 或日志里的 diagnostic_runtime。
- EKF 的 `ldd` 从新 overlay 的标准 install/diagnostic_updater/lib 找到共享库；没有缺失动态库。
- Xacro 展开、launch 参数解析、Python 语法和 shell 语法检查通过。
- 目标场景的 18 个本地资源及 Xacro、OBJ→MTL、DAE 图像引用闭包检查通过；行人 .so 确实构建安装。
- 发现并修复了上游未声明的 champ_config 运行依赖；因此推荐按递归目标包构建，不单独只构建 go2_config。
- 比对确认：本次整理没有改变原行人插件算法、MID-360 模型数值或场景几何/路径参数。SDF 仅将机器专属插件路径改成库名。

## 独立运行验证

使用独立 ROS_DOMAIN_ID=82、独立 Ignition/Gazebo 分区，启动新检出的场景，软件渲染、无 Gazebo GUI、无 RViz。验证后仅关闭这个测试实例；没有发送机器人速度，没有停止用户原有仿真。

15 秒只读采样结果：

- 两个关节控制器 active；12 关节，收到 14,897 条关节状态、744 条里程计。
- 时钟持续推进。
- P1/P2/P3 实际 XY 范围分别约 3.314 / 2.814 / 2.614 m。
- 收到 127 帧点云，现实时间约 8.63 Hz，消息时间戳统计约 8.69 Hz。
- 点云尺寸与字段保持 720×40、mid360_link、x/y/z/intensity/ring。
- 综合只读检查 passed=true。

无窗口结果与原双窗口结果负载条件不同。配置 update_rate=10 Hz；需要进一步测量仿真速度、发布/接收链路和调度才可分析频率差异，当前没有据此给出丢帧结论。

## 尚未验证

- 另一台电脑、全新 Ubuntu 安装、不同显卡或驱动上的完整流程。
- 发布版本在另一台电脑的图形窗口；原机器旧工作空间已验证 GUI，新检出本次使用隔离的无窗口运行验证。
- 新检出中的重新运动实测：本次只读验证；运动算法与参数保持原样，运动结果来自原工作空间的实测。
- 真实硬件、Unitree L1、真实 MID-360 扫描轨迹、逐点时间、完整传感器 IMU 桥接、导航和 SLAM。
- 固定全部 apt 包版本的系统镜像；本项目固定源码基线与依赖提交，并明确受支持版本范围。

## 发布前内容检查

保留主仓库历史、上游 remote 与子模块 gitlink；排除 build/install/log/logs、录包、数据库、下载包、缓存、core dump 和私有认证文件。已检查目标资源不被忽略。当前最大单文件约 10.7 MB，不使用 LFS。敏感模式检查没有发现 GitHub token、常见云密钥或私钥。
