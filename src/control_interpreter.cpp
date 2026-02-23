#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "geometry_msgs/msg/twist.hpp"
#include "rclcpp/rclcpp.hpp"

class ControlInterpreter : public rclcpp::Node
{
public:
  ControlInterpreter()
  : Node("control_interpreter"), serial_fd_(-1)
  {
    // Subscribe to human control commands (e.g. from a joystick or teleop node)
    cmd_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
      "cmd_vel_raw", 10,
      std::bind(&ControlInterpreter::cmd_callback, this, std::placeholders::_1));

    // Publish interpreted speed commands for downstream consumers
    speed_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

    // Declare serial port parameter (default: /dev/ttyUSB0)
    this->declare_parameter<std::string>("serial_port", "/dev/ttyUSB0");
    this->declare_parameter<int>("baud_rate", 115200);

    open_serial_port();

    RCLCPP_INFO(this->get_logger(), "Control Interpreter node started");
  }

  ~ControlInterpreter()
  {
    if (serial_fd_ >= 0) {
      close(serial_fd_);
    }
  }

private:
  void open_serial_port()
  {
    std::string port = this->get_parameter("serial_port").as_string();
    int baud = this->get_parameter("baud_rate").as_int();

    serial_fd_ = open(port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_fd_ < 0) {
      RCLCPP_WARN(
        this->get_logger(),
        "Could not open serial port %s: %s. Motor commands will not be sent.",
        port.c_str(), strerror(errno));
      return;
    }

    struct termios options;
    if (tcgetattr(serial_fd_, &options) < 0) {
      RCLCPP_ERROR(
        this->get_logger(),
        "Failed to get serial port attributes: %s", strerror(errno));
      close(serial_fd_);
      serial_fd_ = -1;
      return;
    }

    // Set baud rate
    speed_t baud_rate;
    if (baud == 9600) {
      baud_rate = B9600;
    } else if (baud == 57600) {
      baud_rate = B57600;
    } else if (baud == 115200) {
      baud_rate = B115200;
    } else {
      RCLCPP_WARN(
        this->get_logger(),
        "Unsupported baud rate %d, defaulting to 115200", baud);
      baud_rate = B115200;
    }
    cfsetispeed(&options, baud_rate);
    cfsetospeed(&options, baud_rate);

    // 8N1
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag |= (CLOCAL | CREAD);

    if (tcsetattr(serial_fd_, TCSANOW, &options) < 0) {
      RCLCPP_ERROR(
        this->get_logger(),
        "Failed to set serial port attributes: %s", strerror(errno));
      close(serial_fd_);
      serial_fd_ = -1;
      return;
    }
    RCLCPP_INFO(this->get_logger(), "Opened serial port %s at %d baud", port.c_str(), baud);
  }

  void cmd_callback(const geometry_msgs::msg::Twist::SharedPtr msg)
  {
    // Convert human control input to motor speed commands
    double linear = msg->linear.x;
    double angular = msg->angular.z;

    // Compute left/right motor speeds using differential drive model
    double left_speed = linear - angular;
    double right_speed = linear + angular;

    // Clamp to [-1.0, 1.0]
    left_speed = std::max(-1.0, std::min(1.0, left_speed));
    right_speed = std::max(-1.0, std::min(1.0, right_speed));

    RCLCPP_INFO(
      this->get_logger(),
      "Speed commands — left: %.3f, right: %.3f", left_speed, right_speed);

    // Publish the interpreted speed command
    auto speed_msg = geometry_msgs::msg::Twist();
    speed_msg.linear.x = (left_speed + right_speed) / 2.0;
    speed_msg.angular.z = (right_speed - left_speed) / 2.0;
    speed_pub_->publish(speed_msg);

    // Send speed commands over Serial
    send_serial_command(left_speed, right_speed);
  }

  void send_serial_command(double left_speed, double right_speed)
  {
    if (serial_fd_ < 0) {
      return;
    }

    // Simple ASCII protocol: "L<value>,R<value>\n"
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "L%.3f,R%.3f\n", left_speed, right_speed);
    if (len < 0 || len >= static_cast<int>(sizeof(buf))) {
      RCLCPP_ERROR(this->get_logger(), "Serial command formatting error");
      return;
    }
    if (write(serial_fd_, buf, len) < 0) {
      RCLCPP_WARN(this->get_logger(), "Failed to write to serial port: %s", strerror(errno));
    }
  }

  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_sub_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr speed_pub_;
  int serial_fd_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ControlInterpreter>());
  rclcpp::shutdown();
  return 0;
}
