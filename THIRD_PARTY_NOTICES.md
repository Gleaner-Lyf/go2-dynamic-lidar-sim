# 第三方来源与许可证

本项目保留各目录中的原作者、许可证、package.xml 声明与 Git 历史，不对第三方文件统一重新授权。

| 内容 | 来源 / 固定版本 | 许可证及保留位置 |
|---|---|---|
| 主工程与 Fortress 移植 | https://github.com/rahgirrafi/unitree-go2-ros2 ，ignition，上游基线 `5580296` | 上游根目录没有独立 LICENSE；各 ROS 包的 package.xml 声明 BSD，保留原声明及作者。不要将本仓库解释成对全部上游资产的新授权。 |
| CHAMP 控制框架与包 | https://github.com/chvmp/champ ，随主工程原历史保留 | `champ/LICENSE`、`champ/champ/include/champ/LICENSE`，BSD 3 条款；个别包按 package.xml 自己的声明，例如导航包 Apache 2.0。 |
| Go2 URDF / DAE | https://github.com/unitreerobotics/unitree_ros/tree/master/robots/go2_description ，经上述主工程移植 | 保留原文件与 `robots/descriptions/go2_description/package.xml` 的 BSD 声明；主工程 README 的原始来源记录保留在 `docs/UPSTREAM_README.md`。 |
| Velodyne 子模块 | https://github.com/rahgirrafi/velodyne_simulator_ros2_gz.git ，`073b13d17f7478176492fe367ead10515353d917` | 子模块 `LICENSE`，BSD-3-Clause，保留各包作者与 manifest。 |
| P1 静态人形 OBJ / MTL | Gazebo Fuel，chapulina / Walking actor，版本 2；原始 walk.dae SHA256 `49af0df3a319d1cb8ca2cebf02dbd00f625e5d5bec820bc5e109925b18b65c6e` | CC0-1.0；完整来源、派生步骤及许可证链接见 `robots/configs/go2_config/meshes/pedestrians/SOURCE_LICENSE.txt`。已上传派生 OBJ/MTL，不需要运行时下载 Fuel。 |
| diagnostic_updater 源码备选 | https://github.com/ros/diagnostics ，4.0.7，`a1b95037eeab10592a8268be2a67cadaad6673da` | BSD-3-Clause；通过 `dependencies.repos` 获取，原仓库 LICENSE 随源码保留。 |

P1 网格由原动画的 1.25 秒姿态烘焙得到，是静态网格；当前场景通过行人插件移动和转向它，没有骨骼步行动画。P2/P3、地面、墙体和标记直接使用 SDF 基础几何体，无外部纹理依赖。

当前动态场景所需资源均在源码与固定依赖中。上游保留的其他示例世界可能引用 Fuel/Gazebo 模型库；这些示例不是本项目已验证的复现入口。
