cmake_minimum_required(VERSION 3.8)
project(mars_rover_ros2)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)

# camera_monitor node
add_executable(camera_monitor src/camera_monitor.cpp)
ament_target_dependencies(camera_monitor rclcpp sensor_msgs)

# control_interpreter node
add_executable(control_interpreter src/control_interpreter.cpp)
ament_target_dependencies(control_interpreter rclcpp geometry_msgs)

install(TARGETS
  camera_monitor
  control_interpreter
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
