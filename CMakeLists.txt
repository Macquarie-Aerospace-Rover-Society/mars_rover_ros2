cmake_minimum_required(VERSION 3.8)
project(mars_rover)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(OpenCV REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(pluginlib REQUIRED)
find_package(hardware_interface REQUIRED)

# rover hardware plugin
add_library(mars_rover_ros2 SHARED src/rover_system.cpp)
target_include_directories(mars_rover_ros2 PRIVATE src/include)
ament_target_dependencies(mars_rover_ros2 rclcpp hardware_interface pluginlib)
pluginlib_export_plugin_description_file(hardware_interface rover.xml)

# camera_monitor node
add_executable(camera_monitor src/camera_monitor.cpp)
ament_target_dependencies(camera_monitor rclcpp sensor_msgs cv_bridge)
target_link_libraries(camera_monitor ${OpenCV_LIBS})

# control_interpreter node
# add_executable(control_interpreter src/control_interpreter.cpp)
# ament_target_dependencies(control_interpreter rclcpp geometry_msgs)

install(TARGETS
  mars_rover_ros2
  camera_monitor
  # control_interpreter
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY
  launch
  config
  DESTINATION share/${PROJECT_NAME}
)
install(DIRECTORY
  description
  config
  DESTINATION share/${PROJECT_NAME}
)
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
