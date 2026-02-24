#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include <opencv2/opencv.hpp>
#include <chrono>
#include <map>
#include <cv_bridge/cv_bridge.h>
using namespace std::chrono_literals;
using std::chrono::milliseconds;

struct CameraInfo
{
  int serial_id;
  int fps;
};

class CameraMonitor : public rclcpp::Node
{
public:
  CameraMonitor(): Node("camera_monitor")
  {
    //? Declare parameters for number of cameras and capture interval
    const std::map<std::string, rclcpp::ParameterValue> base_parameters = {
      {"num_cameras", rclcpp::ParameterValue(4)},
      {"capture_interval_ms", rclcpp::ParameterValue(100)},
      {"individual_camera_topics", rclcpp::ParameterValue(true)},
      {"merged_camera_topic", rclcpp::ParameterValue("camera/merged_feed")},
      {"camera_topics", rclcpp::ParameterValue(std::vector<std::string>{})},
      {"merged_feed", rclcpp::ParameterValue("camera/merged_feed")},
      {"fps", rclcpp::ParameterValue(15)},
      {"encoding", rclcpp::ParameterValue(std::string(encoding_))}
    };
    this->declare_parameters("",base_parameters);

    this->get_parameter("num_cameras", num_cameras_);
    this->get_parameter("capture_interval_ms", capture_interval_ms_);
    this->get_parameter("individual_camera_topics", individual_camera_topics_);
    this->get_parameter("merged_camera_topic", merged_camera_topic_);
    this->get_parameter("camera_topics", camera_topics_);
    this->get_parameter("merged_feed", merged_feed_);
    this->get_parameter("fps", publish_fps_);
    this->get_parameter("encoding", encoding_);
    for(int i = 0; i < num_cameras_; ++i)
    {
      cameras.push_back(load_camera_info("CAM" + std::to_string(i)));
    } 
    //? Initialize cameras and start capture timer
    initialise_cameras();

    //? Start timer to capture latest images at specified interval
    capture_timer_ = this->create_wall_timer(milliseconds(capture_interval_ms_),std::bind(&CameraMonitor::capture_latest_images, this));
    RCLCPP_INFO(this->get_logger(),"Started camera capture timer at %d ms interval for %zu cameras",capture_interval_ms_,video_captures.size());
    publish_timer_ = this->create_wall_timer(milliseconds(1000 / publish_fps_), std::bind(&CameraMonitor::publish_images, this));
    //?start publishing topics
    
    if (individual_camera_topics_)
    {
      for (int i = 0; i < num_cameras_; ++i)
      {
        std::string topic = "camera" + std::to_string(i) + "/image_raw";
        if (i < static_cast<int>(camera_topics_.size()) && !camera_topics_[i].empty())
        {
          topic = camera_topics_[i];
        }
        image_publishers_.push_back(this->create_publisher<sensor_msgs::msg::Image>(topic+"/image_raw", 10));
      }
    }
    merged_publisher_ = this->create_publisher<sensor_msgs::msg::Image>(merged_camera_topic_+"/image_raw", 10);


    }

    /**
     * @brief Destructor
     * 
     */
    ~CameraMonitor()
    {
      for(auto & cap : video_captures)
      {
        if(cap.isOpened())
        {
          cap.release();
        }
      }
    }


private:
  CameraInfo load_camera_info(std::string camera_id = "")
  {
    this->declare_parameters<int>(camera_id, {{"serial_id", 0}, {"fps", 0}});
    CameraInfo camera_info;
    this->get_parameter(camera_id + ".serial_id", camera_info.serial_id);
    this->get_parameter(camera_id + ".fps", camera_info.fps);
    return camera_info;
  }
  void initialise_cameras()
  {
    for(auto & camera : cameras)
    {
      RCLCPP_INFO(this->get_logger(), "Camera Serial ID: %d, FPS: %d", camera.serial_id, camera.fps);
      cv::VideoCapture cap(camera.serial_id);
      if (!cap.isOpened()) {
        RCLCPP_ERROR(this->get_logger(), "Error opening camera with serial ID: %d", camera.serial_id);
        
        }
      else{
          RCLCPP_INFO(this->get_logger(), "Successfully opened camera with serial ID: %d", camera.serial_id);
          video_captures.push_back(std::move(cap));
          latest_frames_.emplace_back();
        }
    }
  }

  void capture_latest_images()
  {
    for (size_t i = 0; i < video_captures.size(); ++i)
    {
      cv::Mat frame;
      if (!video_captures[i].read(frame) || frame.empty())
      {
        RCLCPP_WARN_THROTTLE(this->get_logger(),*this->get_clock(),5000,"Failed to read frame from camera index %zu",i);
      }
      else{
        latest_frames_[i] = frame;
      }

    }
  }
  void publish_images()
  {
    for (size_t i = 0; i < latest_frames_.size(); ++i)
    {
      if (!latest_frames_[i].empty())
      {
        auto msg = cv_bridge::CvImage(std_msgs::msg::Header(), encoding_, latest_frames_[i]).toImageMsg();
        if (individual_camera_topics_ && i < image_publishers_.size())
        {
          image_publishers_[i]->publish(*msg);
        }
      }
    }
    //? Publish merged feed
    if (!latest_frames_.empty())
    {
      cv::Mat merged_frame = merge_frames(latest_frames_);
      auto merged_msg = cv_bridge::CvImage(std_msgs::msg::Header(), encoding_, merged_frame).toImageMsg();
      merged_publisher_->publish(*merged_msg);
    }
  }
  
  cv::Mat merge_frames(const std::vector<cv::Mat>& frames)
  {
    if (frames.empty()) return cv::Mat();
    int num_images = frames.size();
    int grid_cols = std::ceil(std::sqrt(num_images));
    int grid_rows = std::ceil(static_cast<double>(num_images) / grid_cols);
    //create the blank canvas for merged image
    cv::Mat merged_image(frames[0].rows * grid_rows, frames[0].cols * grid_cols, frames[0].type(), cv::Scalar(0, 0, 0));

    for (int i = 0; i < num_images; ++i)
    {
      int row = i / grid_cols;
      int col = i % grid_cols;
      if (!frames[i].empty())
      {
        frames[i].copyTo(merged_image(cv::Rect(col * frames[i].cols, row * frames[i].rows, frames[i].cols, frames[i].rows)));
      }
    }
    return merged_image;
  }

  std::vector<CameraInfo> cameras;
  std::vector<cv::VideoCapture> video_captures;
  std::vector<cv::Mat> latest_frames_;

  rclcpp::TimerBase::SharedPtr capture_timer_;
  rclcpp::TimerBase::SharedPtr publish_timer_;
  std::vector<rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr> image_publishers_;
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr merged_publisher_;



  int num_cameras_;
  int capture_interval_ms_;
  int publish_fps_;
  bool individual_camera_topics_;
  std::string merged_camera_topic_;
  std::vector<std::string> camera_topics_;
  std::string merged_feed_;
  std::string encoding_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<CameraMonitor>());
  rclcpp::shutdown();
  return 0;
}
