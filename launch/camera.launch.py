#!/usr/bin/env python3

from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import PathJoinSubstitution
import os


def generate_launch_description():
    """Launch the camera_monitor node with camera configuration."""
    
    # Get the package share directory
    pkg_share = get_package_share_directory('mars_rover')
    realsense_pkg_share = get_package_share_directory('realsense_ros2_camera')
    # Path to the camera config file
    camera_config_file = os.path.join(pkg_share, 'config','camera', 'camera_config.yaml')
    
    realsense_config_file = os.path.join(pkg_share, 'config', 'camera', 'realsense.yaml')
    
    
    # Create the camera monitor node
    # camera_monitor_node = Node(
    #     package='mars_rover',
    #     executable='camera_monitor',
    #     name='camera_monitor',
    #     output='screen',
    #     emulate_tty=True,
    #     parameters=[camera_config_file]
    # )
    forward_realsense_launch = IncludeLaunchDescription(
      PythonLaunchDescriptionSource([
            PathJoinSubstitution([
                realsense_pkg_share,
                'examples',
                'launch_params_from_file',
                'rs_launch_get_params_from_yaml.py'
            ])
        ]),
        launch_arguments={
            'config_file':realsense_config_file,
            'camera_name': 'forward',
            "camera_namespace": 'camera'
        }.items()
    )
    image_to_laserscan_node = Node(
        package='depth_image_to_laserscan',
        executable='depth_image_to_laserscan_node',
        name='depth_image_to_laserscan',
        output='screen',
        parameters=[{
            'output_frame_id': 'camera_link',
            'range_min': 0.1,
            'range_max': 10.0,
            'scan_height': 10,
        }],
        remappings=[
            ('/depth_image', '/camera/forward/depth/image_rect_raw'),
            # ('/scan', '/nav/camera/forward/scan') 
        ]
    )
    static_forward_transform_node = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='static_transform_publisher',
        output='screen',
        arguments=[
            '0.1', '0', '-0.15',  # x, y, z
            '0', '0', '0',  # roll, pitch, yaw
            'odom',  # parent frame
            'forward_link'  # child frame
        ],
    )

    
    
    return LaunchDescription([
        forward_realsense_launch,
        image_to_laserscan_node,
        static_forward_transform_node,
        # camera_monitor_node
    ])
