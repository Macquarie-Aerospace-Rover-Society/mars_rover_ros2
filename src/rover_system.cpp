// Copyright 2021 ros2_control Development Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "rover_system.hpp"

#include <chrono>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <vector>

#include "hardware_interface/lexical_casts.hpp"
#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "rclcpp/rclcpp.hpp"

namespace mars_rover
{

hardware_interface::CallbackReturn RoverSystemHardware::on_init(
  const hardware_interface::HardwareInfo & info)
{
  if (
    hardware_interface::SystemInterface::on_init(info) != hardware_interface::CallbackReturn::SUCCESS)
  {
    return hardware_interface::CallbackReturn::ERROR;
  }

  logger_ = std::make_shared<rclcpp::Logger>(rclcpp::get_logger("controller_manager.resource_manager.hardware_component.system.RoverSystem"));
  clock_ = std::make_shared<rclcpp::Clock>(rclcpp::Clock());


  if (!load_hardware_parameters(info_, params)) {
    RCLCPP_ERROR(get_logger(), "Failed to initialize hardware due to missing parameters.");
    return hardware_interface::CallbackReturn::ERROR;
  }

  serial_port_ = params["serial_port"];
  try {
    baud_rate_ = std::stoi(params["baud_rate"]);
  } catch (const std::exception & e) {
    RCLCPP_ERROR(get_logger(),"Invalid hardware parameter 'baud_rate': '%s' (%s)",params["baud_rate"].c_str(),e.what());
    return hardware_interface::CallbackReturn::ERROR;
  }

  try
  {
    serial_connection_ = std::make_unique<serial::Serial>(serial_port_, baud_rate_, serial::Timeout::simpleTimeout(1000));
    RCLCPP_INFO(get_logger(),"control system connecting to serial port %s with baud rate %d", serial_port_.c_str(), baud_rate_);
  }
  catch(const std::exception& e)
  {
    RCLCPP_ERROR(get_logger(), "Failed to connect to serial port: %s", e.what());
    // return hardware_interface::CallbackReturn::ERROR;
  }
  
  

    
  hw_positions_.resize(info_.joints.size(), std::numeric_limits<double>::quiet_NaN());
  hw_velocities_.resize(info_.joints.size(), std::numeric_limits<double>::quiet_NaN());
  hw_commands_.resize(info_.joints.size(), std::numeric_limits<double>::quiet_NaN());

    

  for (const hardware_interface::ComponentInfo & joint : info_.joints)
  {


  }

  return hardware_interface::CallbackReturn::SUCCESS;
}

std::vector<hardware_interface::StateInterface> RoverSystemHardware::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface> state_interfaces;
  for (auto i = 0u; i < info_.joints.size(); i++)
  {
    state_interfaces.emplace_back(
      hardware_interface::StateInterface(
        info_.joints[i].name, hardware_interface::HW_IF_POSITION, &hw_positions_[i]));
    state_interfaces.emplace_back(
      hardware_interface::StateInterface(
        info_.joints[i].name, hardware_interface::HW_IF_VELOCITY, &hw_velocities_[i]));
  }

  return state_interfaces;
}

std::vector<hardware_interface::CommandInterface> RoverSystemHardware::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface> command_interfaces;
  for (auto i = 0u; i < info_.joints.size(); i++)
  {
    command_interfaces.emplace_back(
      hardware_interface::CommandInterface(
        info_.joints[i].name, hardware_interface::HW_IF_VELOCITY, &hw_commands_[i]));
  }

  return command_interfaces;
}

hardware_interface::CallbackReturn RoverSystemHardware::on_activate(const rclcpp_lifecycle::State & /*previous_state*/)
{
  for (auto i = 0u; i < hw_positions_.size(); i++) {
    if (std::isnan(hw_positions_[i])) {
      hw_positions_[i] = 0.0;
      hw_velocities_[i] = 0.0;
      hw_commands_[i] = 0.0;
    }
  }
  log_robot_stats();
  RCLCPP_INFO(get_logger(), "Successfully activated!");
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn RoverSystemHardware::on_deactivate(const rclcpp_lifecycle::State & /*previous_state*/)
{
    // TODO
    RCLCPP_INFO(get_logger(), "Successfully deactivated!");
    return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::return_type RoverSystemHardware::read(const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  if(!serial_connection_ || !serial_connection_->isOpen()) {
    RCLCPP_ERROR_THROTTLE(get_logger(), *clock_, 2000, "Serial connection is not open");
    return hardware_interface::return_type::OK;
  }
  auto msg = serial_connection_->read(hw_positions_.size() * sizeof(double) * 2); // read positions and velocities as bytes, you can change this to match the expected format of your robot
  hw_positions_.clear();
  hw_velocities_.clear();
  for(int i=0; i<msg.size(); i+=2) {
    double pos = static_cast<double>(msg[i]);
    double vel = static_cast<double>(msg[i + 1]);
    hw_positions_.push_back(pos);
    hw_velocities_.push_back(vel);
  }

  if(hw_positions_.size() != info_.joints.size() || hw_velocities_.size() != info_.joints.size()) {
    RCLCPP_ERROR(get_logger(), "Expected %zu joint states, but got %zu positions and %zu velocities", info_.joints.size(), hw_positions_.size(), hw_velocities_.size());
    return hardware_interface::return_type::ERROR;
  }

  
  // TODO set hw_positions_ and hw_velocities_ from the robot
  return hardware_interface::return_type::OK;
}



hardware_interface::return_type RoverSystemHardware::write(const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
    if(!serial_connection_ || !serial_connection_->isOpen()) {
  RCLCPP_ERROR_THROTTLE(get_logger(), *clock_, 2000, "Serial connection is not open");
    return hardware_interface::return_type::OK;
  }
  if (hw_commands_.size() < 4) {
    RCLCPP_ERROR(get_logger(), "Expected 4 commands for the rover, but got %zu", hw_commands_.size());
    return hardware_interface::return_type::OK;
  }
  // currently just casting the commands to uint8_t and sending them as bytes, but you can change this to match the expected format of your robot
  std::vector<uint8_t> msg = std::vector<uint8_t>{static_cast<uint8_t>(hw_commands_[0]), static_cast<uint8_t>(hw_commands_[1]), static_cast<uint8_t>(hw_commands_[2]), static_cast<uint8_t>(hw_commands_[3])};
  serial_connection_->write(msg);

  // TODO set the robot's velocity from hw_commands_
  return hardware_interface::return_type::OK;
}

}  // namespace mars_rover

#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(mars_rover::RoverSystemHardware, hardware_interface::SystemInterface)
