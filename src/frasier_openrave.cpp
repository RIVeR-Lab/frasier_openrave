#include <frasier_openrave/frasier_openrave.h>


FRASIEROpenRAVE::FRASIEROpenRAVE(ros::NodeHandle n, bool run_viewer, bool real_robot) : nh_(n),
                                                                                        run_viewer_flag_(run_viewer),
                                                                                        run_joint_updater_flag_(real_robot){

  // ROS
  joint_state_topic_ = "/hsrb/robot_state/joint_states";
  base_state_topic_ = "/hsrb/pose2D";

  joint_state_sub_ = nh_.subscribe(joint_state_topic_, 1, &FRASIEROpenRAVE::jointSensorCb, this);
  base_state_sub_ = nh_.subscribe(base_state_topic_, 1, &FRASIEROpenRAVE::baseStateCb, this);
  robot_name_ = "hsrb";

  package_path_ = ros::package::getPath("frasier_openrave");
  worlds_path_ = package_path_ + "/worlds/";
  config_path_ = package_path_ +"/config/";

  // OpenRAVE
  OpenRAVE::RaveInitialize(true);
  env_ = OpenRAVE::RaveCreateEnvironment();
  assert(env_);
  env_->SetDebugLevel(OpenRAVE::Level_Error);

  joint_state_flag_ = false;
  plan_plotter_ = false;

  base_link_ = "base_link";

  joint_names_ = {"base_x_joint", "base_y_joint", "base_t_joint", "arm_lift_joint", "arm_flex_joint", "arm_roll_joint",
                  "wrist_flex_joint", "wrist_roll_joint", "hand_motor_joint", "hand_l_spring_proximal_joint",
                  "hand_l_distal_joint", "hand_r_spring_proximal_joint", "hand_r_distal_joint", "head_pan_joint", "head_tilt_joint"};

  whole_body_joint_names_ = {"base_x_joint", "base_y_joint", "base_t_joint", "arm_lift_joint", "arm_flex_joint",
                             "arm_roll_joint", "wrist_flex_joint", "wrist_roll_joint"};

}

FRASIEROpenRAVE::~FRASIEROpenRAVE(){
  env_->Destroy();


  if (run_joint_updater_flag_) {
    joint_state_thread_.join();
  }

  if (run_viewer_flag_) {
    viewer_->quitmainloop();
    viewer_thread_.join();
  }


}

void FRASIEROpenRAVE::jointSensorCb(const sensor_msgs::JointState::ConstPtr &msg){
  boost::mutex::scoped_lock lock(joint_state_mutex_);
  joints_ = *msg;
  joint_state_flag_ = true;

}

void FRASIEROpenRAVE::baseStateCb(const geometry_msgs::Pose2D::ConstPtr &msg){
  boost::mutex::scoped_lock lock(base_state_mutex_);
  base_ = *msg;


}

sensor_msgs::JointState FRASIEROpenRAVE::getWholeBodyState() { //TODO: Try without base_roll_joint

    sensor_msgs::JointState state;

    state.name.push_back("odom_x");
    state.name.push_back("odom_y");
    state.name.push_back("odom_t");
    state.name.push_back("arm_lift_joint");
    state.name.push_back("arm_flex_joint");
    state.name.push_back("arm_roll_joint");
    state.name.push_back("wrist_flex_joint");
    state.name.push_back("wrist_roll_joint");
    state.name.push_back("base_roll_joint");


    state.position.push_back(base_.x);
    state.position.push_back(base_.y);
    state.position.push_back(base_.theta);
    state.position.push_back(joints_.position[12]);
    state.position.push_back(joints_.position[13]);
    state.position.push_back(joints_.position[14]);
    state.position.push_back(joints_.position[15]);
    state.position.push_back(joints_.position[16]);
    state.position.push_back(joints_.position[0]); // /filter_hsrb_trajectory srv needs base_roll_joint

    return state;
}

bool FRASIEROpenRAVE::loadHSR() { // TODO: Check if env exist

  std::string world_path;
  world_path = worlds_path_ + "hsr_empty_world.xml";
  bool success = env_->Load(world_path);

//  planning_env_ = env_->CloneSelf(OpenRAVE::Clone_Bodies);

  // Get robot
  hsr_ = env_->GetRobot(robot_name_);

  // Get manipulator
  manip_ = hsr_->GetManipulator("whole_body");


  if (success) {
    std::cout << "RAVE: HSR initialized!" << std::endl;
  }



  return success;
}

bool FRASIEROpenRAVE::loadCustomEnv() {
  std::string world_path;
  world_path = worlds_path_ + "hsr_table_shelf.xml";
  bool success = env_->Load(world_path);

  hsr_ = env_->GetRobot(robot_name_);

  // Get manipulator
  manip_ = hsr_->GetManipulator("whole_body");


  if (success) {
    std::cout << "RAVE: custom world initialized!" << std::endl;
  }


}

void FRASIEROpenRAVE::updatePlanningEnv() {
  std::vector<double> q;
  hsr_->GetActiveDOFValues(q);

  planning_env_->GetRobot(robot_name_)->SetJointValues(q);

}


void FRASIEROpenRAVE::setViewer(){
  // OpenRAVE::EnvironmentMutex::scoped_lock lockenv(env_->GetMutex());

  std::string viewer_name = OpenRAVE::RaveGetDefaultViewerType(); // qtcoin

  viewer_ = OpenRAVE::RaveCreateViewer(env_, viewer_name);
  BOOST_ASSERT(!!viewer_);

  env_->Add(viewer_);

  // viewer_.SetBkgndColor() TODO: Change background colour

  viewer_->main(true); //TODO: Need EnvironmentMutex ???


}



void FRASIEROpenRAVE::getActiveJointIndex(std::vector<int>& q_index){
  for (int i = 0; i < joint_names_.size(); i++) {
    q_index.push_back(hsr_->GetJoint(joint_names_[i])->GetDOFIndex());
  }
}

void FRASIEROpenRAVE::getWholeBodyJointIndex(std::vector<int>& q_index){
  for (int i = 0; i < whole_body_joint_names_.size(); i++) {
    q_index.push_back(hsr_->GetJoint(whole_body_joint_names_[i])->GetDOFIndex());
  }
}

OpenRAVE::Transform FRASIEROpenRAVE::getRobotTransform() {
  return hsr_->GetLink(base_link_)->GetTransform();
}

void FRASIEROpenRAVE::updateJointStates(){

  ros::Rate r(50);
  std::vector<int> q_index;
  std::vector<double> q(hsr_->GetDOF(), 0.0);
  std::vector<double> q_v(hsr_->GetDOF(), 0.0);
  std::vector<std::string> q_names;

  hsr_->GetActiveDOFValues(q);
  hsr_->GetActiveDOFVelocities(q_v);
  getActiveJointIndex(q_index);

  std::cout << "RAVE: started updating joints!" << '\n';

  while (ros::ok()) {

    if (joint_state_flag_) {

      {
        boost::mutex::scoped_lock lock(joint_state_mutex_);
        {
          boost::mutex::scoped_lock lock(base_state_mutex_);

          // Set positions // TODO: Fix minus theta 
          q[q_index[0]] = base_.x;
          q[q_index[1]] = base_.y;
          q[q_index[2]] = base_.theta;


          q[q_index[3]] = joints_.position[12];
          q[q_index[4]] = joints_.position[13];
          q[q_index[5]] = joints_.position[14];
          q[q_index[6]] = joints_.position[15];
          q[q_index[7]] = joints_.position[16];
          q[q_index[8]] = joints_.position[17];
          q[q_index[9]] = joints_.position[19];
          q[q_index[10]] = joints_.position[21];
          q[q_index[11]] = joints_.position[23];
          q[q_index[12]] = joints_.position[25];
          q[q_index[13]] = joints_.position[10];
          q[q_index[14]] = joints_.position[11];


          // Set velocities TODO: Set velocities
          // q[q_index[0]] = base_.x;
          // q[q_index[1]] = base_.y;
          // q[q_index[2]] = base_.theta;
          //
          q_v[q_index[3]] = joints_.velocity[12];
          q_v[q_index[4]] = joints_.velocity[13];
          q_v[q_index[5]] = joints_.velocity[14];
          q_v[q_index[6]] = joints_.velocity[15];
          q_v[q_index[7]] = joints_.velocity[16];
          q_v[q_index[8]] = joints_.velocity[17];
          q_v[q_index[9]] = joints_.velocity[19];
          q_v[q_index[10]] = joints_.velocity[21];
          q_v[q_index[11]] = joints_.velocity[23];
          q_v[q_index[12]] = joints_.velocity[25];
          q_v[q_index[13]] = joints_.velocity[10];
          q_v[q_index[14]] = joints_.velocity[11];
        }

      }


       hsr_->SetActiveDOFValues(q);
//      hsr_->SetJointValues(q);
       hsr_->SetActiveDOFVelocities(q_v);

    }

//    ros::spinOnce();
    r.sleep();
  }

}

void FRASIEROpenRAVE::startThreads(/* arguments */) {
  std::cout << "RAVE: starting threads..." << std::endl;

  if (run_joint_updater_flag_) {
    joint_state_thread_ = boost::thread(&FRASIEROpenRAVE::updateJointStates, this);
  }

  if (run_viewer_flag_) {
    viewer_thread_ = boost::thread(&FRASIEROpenRAVE::setViewer, this);
  }

}