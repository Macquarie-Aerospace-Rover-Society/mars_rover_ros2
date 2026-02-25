import os

from click import launch
from ament_index_python import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, RegisterEventHandler
from launch.conditions import IfCondition
from launch.event_handlers import OnProcessExit
from launch.substitutions import Command, FindExecutable, PathJoinSubstitution, LaunchConfiguration
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, RegisterEventHandler
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    # Declare arguments
    declared_arguments = []
    declared_arguments.append(
        DeclareLaunchArgument(
            "joy_con",
            default_value="xbox",
            description="Joystick configuration (e.g., xbox, ps3, atk3, joy-con).",
        )
    )
    
    # Initialize Arguments
    joy_con = LaunchConfiguration("joy_con")
    
    teleop = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            PathJoinSubstitution([
                get_package_share_directory("teleop_twist_joy"),
                'launch',
                'teleop-launch.py'
            ])
        ]),
        launch_arguments={
            'joy_config': joy_con
        }.items()
    )
    launch_files = [teleop]
    
    return LaunchDescription(declared_arguments + launch_files)