#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"

class CameraMonitor : public rclcpp::Node
{
public:
  CameraMonitor()
  : Node("camera_monitor")
  {
    // Subscribe to camera feed topic
    camera_sub_ = this->create_subscription<sensor_msgs::msg::Image>(
      "camera/image_raw", 10,
      std::bind(&CameraMonitor::camera_callback, this, std::placeholders::_1));

    RCLCPP_INFO(this->get_logger(), "Camera Monitor node started");
  }

private:
  void camera_callback(const sensor_msgs::msg::Image::SharedPtr msg)
  {
    RCLCPP_INFO(
      this->get_logger(),
      "Received image: %dx%d, encoding: %s",
      msg->width, msg->height, msg->encoding.c_str());
  }

  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr camera_sub_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<CameraMonitor>());
  rclcpp::shutdown();
  return 0;
}
