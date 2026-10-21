cmake_minimum_required(VERSION 3.5)
project(champ_gazebo)

add_compile_options(-std=c++17)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(urdf REQUIRED)
find_package(champ REQUIRED)
find_package(champ_msgs REQUIRED)
find_package(ros_gz_sim REQUIRED)
find_package(gz_ros2_control REQUIRED)
find_package(ignition-gazebo6 REQUIRED)
find_package(ignition-plugin1 REQUIRED COMPONENTS register)

set(IGN_GAZEBO_VER ${ignition-gazebo6_VERSION_MAJOR})
set(IGN_PLUGIN_VER ${ignition-plugin1_VERSION_MAJOR})

set(dependencies
  rclcpp
  urdf
  champ
  champ_msgs
  ros_gz_sim
  gz_ros2_control
  ignition-gazebo6
  ignition-plugin1
)

install(PROGRAMS
  scripts/imu_sensor.py
  scripts/odometry_tf.py
  scripts/odometry.py
  DESTINATION bin
)

include_directories(
  include
  ${champ_INCLUDE_DIRS}
  ${champ_INCLUDE_DIRS}/champ/
  ${champ_INCLUDE_DIRS}/champ/champ/
)

add_library(PedestrianPathController SHARED
  src/pedestrian_path_controller.cpp
)

target_link_libraries(PedestrianPathController PUBLIC
  ignition-gazebo${IGN_GAZEBO_VER}::ignition-gazebo${IGN_GAZEBO_VER}
  ignition-plugin${IGN_PLUGIN_VER}::ignition-plugin${IGN_PLUGIN_VER}
)

# NOTE: contact_sensor.cpp uses Gazebo Classic APIs and is commented out
# for Ignition Fortress. A new implementation using Ignition transport
# would be needed. For now, contact detection can be handled differently.
# add_executable(contact_sensor src/contact_sensor.cpp)
# target_link_libraries(contact_sensor ${GAZEBO_LIBRARIES})
# ament_target_dependencies(contact_sensor ${dependencies})

# install(TARGETS 
#   contact_sensor
#     DESTINATION lib/${PROJECT_NAME}
# )
install(TARGETS
  PedestrianPathController
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})
install(DIRECTORY config DESTINATION share/${PROJECT_NAME})
install(DIRECTORY worlds DESTINATION share/${PROJECT_NAME})

ament_export_include_directories(include)
ament_export_libraries(PedestrianPathController)
ament_export_dependencies(${dependencies})

ament_package()
