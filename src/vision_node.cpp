#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <opencv2/highgui/highgui.hpp>
#include <cv_bridge/cv_bridge.h>

using namespace cv;

class SudokuROS
{
private:

public:

};


int main(int argc, char** argv)
{
  ros::init(argc, argv, "vision_node");

  ros::NodeHandle nh;

  image_transport::ImageTransport it(nh);

  auto pub = it.advertise("camera/image", 81);

  auto image = cv::imread(argv[1], CV_LOAD_IMAGE_COLOR);

  cv::waitKey(30);

  auto msg = cv_bridge::CvImage(std_msgs::Header(), "bgr8", image).toImageMsg();

  ros::Rate loop_rate(5);


  while (nh.ok()) {

    pub.publish(msg);

    ros::spinOnce();

    loop_rate.sleep();
  }
}
