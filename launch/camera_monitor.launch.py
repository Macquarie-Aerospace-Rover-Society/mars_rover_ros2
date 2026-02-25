#!/usr/bin/env python3

from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    """Launch the camera_monitor node with camera configuration."""
    
    # Get the package share directory
    pkg_share = get_package_share_directory('mars_rover')
    
    # Path to the camera config file
    camera_config_file = os.path.join(pkg_share, 'config', 'camera_config.yaml')
    
    # Create the camera monitor node
    camera_monitor_node = Node(
        package='mars_rover',
        executable='camera_monitor',
        name='camera_monitor',
        output='screen',
        emulate_tty=True,
        parameters=[camera_config_file]
    )
    
    return LaunchDescription([
        camera_monitor_node
    ])
