// Copyright 2016 Open Source Robotics Foundation, Inc.
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

#include <algorithm>
#include <vector>

#include <ros/ros.h>

#include <nist_gear/LogicalCameraImage.h>
#include <nist_gear/Order.h>
#include <nist_gear/Proximity.h>
#include <sensor_msgs/LaserScan.h>
#include <sensor_msgs/Range.h>
#include <std_msgs/Float32.h>
#include <std_msgs/String.h>
#include <std_srvs/Trigger.h>
#include <tf2_ros/transform_listener.h>
#include <geometry_msgs/TransformStamped.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h> //--needed for tf2::Matrix3x3
#include <boost/bind.hpp>

/**
 * @brief Start the competition
 * Create a service client to /ariac/start_competition
 */
void start_competition(ros::NodeHandle &node)
{
  // Create a Service client for the correct service, i.e. '/ariac/start_competition'.
  ros::ServiceClient start_client =
      node.serviceClient<std_srvs::Trigger>("/ariac/start_competition");
  // If it's not already ready, wait for it to be ready.
  // Calling the Service using the client before the server is ready would fail.
  if (!start_client.exists())
  {
    ROS_INFO("Waiting for the competition to be ready...");
    start_client.waitForExistence();
    ROS_INFO("Competition is now ready.");
  }
  ROS_INFO("Requesting competition start...");
  std_srvs::Trigger srv;  // Combination of the "request" and the "response".
  start_client.call(srv); // Call the start Service.
  if (!srv.response.success)
  { // If not successful, print out why.
    ROS_ERROR_STREAM("Failed to start the competition: " << srv.response.message);
  }
  else
  {
    ROS_INFO("Competition started!");
  }
}

/**
 * @brief A simple competition class.
 * 
 * This class can hold state and provide methods that handle incoming data.
 * 
 */
class MyCompetitionClass
{
public:
  explicit MyCompetitionClass(ros::NodeHandle &node)
      : current_score_(0)
  {
  }

  /**
   * @brief Called when a new Message is received on the Topic /ariac/current_score
   * 
   * This function sets the value of the attribute current_score_ 
   * @param msg Message used to set the state of the competition.
   */
  void current_score_callback(const std_msgs::Float32::ConstPtr &msg)
  {
    if (msg->data != current_score_)
    {
      ROS_INFO_STREAM("Score: " << msg->data);
    }
    current_score_ = msg->data;
  }

  /**
   * @brief Called when a new Message is received on /ariac/competition_state
   * 
   * This function sets the state of the competition to 'done'.
   * @param msg Message used to set the state of the competition.
   */
  void competition_state_callback(const std_msgs::String::ConstPtr &msg)
  {
    if (msg->data == "done" && competition_state_ != "done")
    {
      ROS_INFO("Competition ended.");
    }
    competition_state_ = msg->data;
  }

  /**
   * @brief Called when a new Message is received on the Topic /ariac/orders
   * 
   * This function adds the Message to received_orders_
   * 
   * @param msg Message containing information on the order.
   */
  void order_callback(const nist_gear::Order::ConstPtr &msg)
  {
    ROS_INFO_STREAM("Received order:\n" << *msg);
    received_orders_.push_back(*msg);
  }

  /**
   * @brief Called when a new Message is received on the Topic /ariac/logical_camera_x
   * 
   * This function reports the number of objects detected by a logical camera.
   * 
   * @param msg Message containing information on objects detected by the camera.
   */
  //void logical_camera_callback(const nist_gear::LogicalCameraImage::ConstPtr &msg, int cam_idx){
  void logical_camera_callback(
      const nist_gear::LogicalCameraImage::ConstPtr &msg, int id)
  {
    ROS_INFO_STREAM_THROTTLE(10, "Logical camera: " << msg->models.size() << " objects");

    if(msg->models.size() > 0) {
      tf2_ros::Buffer tfBuffer;
      tf2_ros::TransformListener tfListener(tfBuffer);

      ros::Rate rate(10);
      ros::Duration timeout(5.0);
      
      geometry_msgs::TransformStamped transformStamped;
      geometry_msgs::PoseStamped pose_in_world, pose_in_reference;

      try {
        	transformStamped = tfBuffer.lookupTransform("world", "logical_camera_" + std::to_string(id) + "_frame",
                                ros::Time(0), timeout);
      }
      catch (tf2::TransformException &ex) {
        ROS_WARN("%s",ex.what());
        ros::Duration(1.0).sleep();
      }

      for(int i=0; i<msg->models.size(); i++) {
        pose_in_reference.pose = msg->models[i].pose;
        tf2::doTransform(pose_in_reference, pose_in_world, transformStamped);

        tf2::Quaternion q(
          pose_in_world.pose.orientation.x,
          pose_in_world.pose.orientation.y,
          pose_in_world.pose.orientation.z,
          pose_in_world.pose.orientation.w
        );

        tf2::Matrix3x3 m(q);
        double roll, pitch, yaw;
        m.getRPY(roll, pitch, yaw);

        ROS_INFO("%s, world frame co-ordinates: [%f,%f,%f] [%f,%f,%f]",
                    msg->models[i].type.c_str(),
                    pose_in_world.pose.position.x,
                    pose_in_world.pose.position.y,
                    pose_in_world.pose.position.z,
                    roll,
                    pitch,
                    yaw);
        
        // tf2::Quaternion q(
        //   transformStamped.transform.rotation.x,
        //   transformStamped.transform.rotation.y,
        //   transformStamped.transform.rotation.z,
        //   transformStamped.transform.rotation.w);
        // tf2::Matrix3x3 m(q);
        // double roll, pitch, yaw;
        // m.getRPY(roll, pitch, yaw);

        // ROS_INFO("%s in world frame: [%f,%f,%f] [%f,%f,%f]", msg->models[i].type,
        // transformStamped.transform.translation.x,
        // transformStamped.transform.translation.y,
        // transformStamped.transform.translation.z,
        // roll,
        // pitch,
        // yaw);
        
      }
      
    }
  }

  /**
   * @brief Called when a new Message is received on the Topic /ariac/break_beam_x_change
   * 
   * This function reports when an object crossed the beam.
   * 
   * @param msg Message of Boolean type returning true if an object crossed the beam.
   */
  void break_beam_callback(const nist_gear::Proximity::ConstPtr &msg)
  {
    if (msg->object_detected) // If there is an object in proximity.
      ROS_WARN("Break beam triggered.");
  }

  void proximity_sensor_callback(const sensor_msgs::Range::ConstPtr &msg)
  {
    if ((msg->max_range - msg->range) > 0.01)
    { // If there is an object in proximity.
      ROS_INFO_THROTTLE(1, "Proximity sensor sees something.");
    }
  }

  void laser_profiler_callback(const sensor_msgs::LaserScan::ConstPtr &msg)
  {
    size_t number_of_valid_ranges = std::count_if(
        msg->ranges.begin(), msg->ranges.end(), [](const float f) { return std::isfinite(f); });
    if (number_of_valid_ranges > 0)
    {
      ROS_INFO_THROTTLE(1, "Laser profiler sees something.");
    }
  }
/*
  void part_report(){
  	for(int i = 0; i < logical_camera_images_.size(); i++){
  		ROS_INFO_STREAM_THROTTLE(10,"models for camera " << i << " :" << logical_camera_images_.at(i).models.size());
  	}
  }
*/
private:
  std::string competition_state_;
  double current_score_;
  std::vector<nist_gear::Order> received_orders_;
  std::vector<nist_gear::LogicalCameraImage> logical_camera_images_;
};

int main(int argc, char **argv)
{
  // Last argument is the default name of the node.
  ros::init(argc, argv, "ariac_example_node");

  ros::NodeHandle node;

  // Instance of custom class from above.
  MyCompetitionClass comp_class(node);

  // Subscribe to the '/ariac/current_score' Topic.
  ros::Subscriber current_score_subscriber = node.subscribe(
      "/ariac/current_score", 10,
      &MyCompetitionClass::current_score_callback, &comp_class);

  // Subscribe to the '/ariac/competition_state' Topic.
  ros::Subscriber competition_state_subscriber = node.subscribe(
      "/ariac/competition_state", 10,
      &MyCompetitionClass::competition_state_callback, &comp_class);

  // Subscribe to the '/ariac/orders' Topic.
  ros::Subscriber orders_subscriber = node.subscribe(
      "/ariac/orders", 10,
      &MyCompetitionClass::order_callback, &comp_class);

  // Subscribe to the '/ariac/range_finder_0' Topic.
  ros::Subscriber proximity_sensor_subscriber = node.subscribe(
      "/ariac/range_finder_0", 
      10, 
      &MyCompetitionClass::proximity_sensor_callback,
      &comp_class);

  // Subscribe to the '/ariac/breakbeam_0_change' Topic.
  ros::Subscriber break_beam_subscriber = node.subscribe(
      "/ariac/breakbeam_0_change", 10,
      &MyCompetitionClass::break_beam_callback,
      &comp_class);

  // Subscribe to the '/ariac/logical_camera_0' Topic.
  ros::Subscriber logical_camera_0_subscriber = node.subscribe<nist_gear::LogicalCameraImage>(
      "/ariac/logical_camera_0", 1000,
      boost::bind(&MyCompetitionClass::logical_camera_callback, &comp_class, _1, 0));

  // Subscribe to the '/ariac/logical_camera_1' Topic.
  ros::Subscriber logical_camera_1_subscriber = node.subscribe<nist_gear::LogicalCameraImage>(
      "/ariac/logical_camera_1", 1000,
      boost::bind(&MyCompetitionClass::logical_camera_callback, &comp_class, _1, 1));

    // Subscribe to the '/ariac/logical_camera_2' Topic.
  ros::Subscriber logical_camera_2_subscriber = node.subscribe<nist_gear::LogicalCameraImage>(
      "/ariac/logical_camera_2", 1000,
      boost::bind(&MyCompetitionClass::logical_camera_callback, &comp_class, _1, 2));

    // Subscribe to the '/ariac/logical_camera_3' Topic.
  ros::Subscriber logical_camera_3_subscriber = node.subscribe<nist_gear::LogicalCameraImage>(
      "/ariac/logical_camera_3", 1000,
      boost::bind(&MyCompetitionClass::logical_camera_callback, &comp_class, _1, 3));

    // Subscribe to the '/ariac/logical_camera_4' Topic.
  ros::Subscriber logical_camera_4_subscriber = node.subscribe<nist_gear::LogicalCameraImage>(
      "/ariac/logical_camera_4", 1000,
      boost::bind(&MyCompetitionClass::logical_camera_callback, &comp_class, _1, 4));

    // Subscribe to the '/ariac/logical_camera_5' Topic.
  ros::Subscriber logical_camera_5_subscriber = node.subscribe<nist_gear::LogicalCameraImage>(
      "/ariac/logical_camera_5", 1000,
      boost::bind(&MyCompetitionClass::logical_camera_callback, &comp_class, _1, 5));

    // Subscribe to the '/ariac/logical_camera_6' Topic.
  ros::Subscriber logical_camera_6_subscriber = node.subscribe<nist_gear::LogicalCameraImage>(
      "/ariac/logical_camera_6", 1000,
      boost::bind(&MyCompetitionClass::logical_camera_callback, &comp_class, _1, 6));

    // Subscribe to the '/ariac/logical_camera_7' Topic.
  ros::Subscriber logical_camera_7_subscriber = node.subscribe<nist_gear::LogicalCameraImage>(
      "/ariac/logical_camera_7", 1000,
      boost::bind(&MyCompetitionClass::logical_camera_callback, &comp_class, _1, 7));

    // Subscribe to the '/ariac/logical_camera_8' Topic.
  ros::Subscriber logical_camera_8_subscriber = node.subscribe<nist_gear::LogicalCameraImage>(
      "/ariac/logical_camera_8", 1000,
      boost::bind(&MyCompetitionClass::logical_camera_callback, &comp_class, _1, 8));

    // Subscribe to the '/ariac/logical_camera_9' Topic.
  ros::Subscriber logical_camera_9_subscriber = node.subscribe<nist_gear::LogicalCameraImage>(
      "/ariac/logical_camera_9", 1000,
      boost::bind(&MyCompetitionClass::logical_camera_callback, &comp_class, _1, 9));

    // Subscribe to the '/ariac/logical_camera_10' Topic.
  ros::Subscriber logical_camera_10_subscriber = node.subscribe<nist_gear::LogicalCameraImage>(
      "/ariac/logical_camera_10", 100,
      boost::bind(&MyCompetitionClass::logical_camera_callback, &comp_class, _1, 10));

    // Subscribe to the '/ariac/logical_camera_11' Topic.
  ros::Subscriber logical_camera_11_subscriber = node.subscribe<nist_gear::LogicalCameraImage>(
      "/ariac/logical_camera_11", 1000,
      boost::bind(&MyCompetitionClass::logical_camera_callback, &comp_class, _1, 11));

    // Subscribe to the '/ariac/logical_camera_12' Topic.
  ros::Subscriber logical_camera_12_subscriber = node.subscribe<nist_gear::LogicalCameraImage>(
      "/ariac/logical_camera_12", 1000,
      boost::bind(&MyCompetitionClass::logical_camera_callback, &comp_class, _1, 12));

      // Subscribe to the '/ariac/logical_camera_13' Topic.
  ros::Subscriber logical_camera_13_subscriber = node.subscribe<nist_gear::LogicalCameraImage>(
      "/ariac/logical_camera_13", 1000,
      boost::bind(&MyCompetitionClass::logical_camera_callback, &comp_class, _1, 13));

  // Subscribe to the '/ariac/laser_profiler_0' Topic.
  ros::Subscriber laser_profiler_subscriber = node.subscribe(
      "/ariac/laser_profiler_0", 10, &MyCompetitionClass::laser_profiler_callback, &comp_class);

  //comp_class.part_report();

  ROS_INFO("Setup complete.");
  start_competition(node);

  ros::spin();
  
  return 0;
}
