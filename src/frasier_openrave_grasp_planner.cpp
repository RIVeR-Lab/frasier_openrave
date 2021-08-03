#include <frasier_openrave/frasier_openrave.h>

////////////////////////////// GRASPING //////////////////////////////

void FRASIEROpenRAVE::sampleGraspPoses(std::string &obj_name) {
    OpenRAVE::KinBodyPtr obj_kinbody = env_->GetKinBody(obj_name);
    OpenRAVE::Transform obj_pose = obj_kinbody->GetTransform();
    OpenRAVE::KinBody::Link::GeometryPtr obj_geom = obj_kinbody->GetLink("base")->GetGeometry(0);
//  drawTransform(obj_pose);

    if (obj_geom->GetType() == OpenRAVE::GeometryType::GT_Cylinder) {
        std::cout << "RAVE: sampling grasp poses for cylinder type object" << std::endl;

        double H = obj_geom->GetCylinderHeight();
        double h = H / 10.0;
        double h_bottom = obj_pose.trans.z - H / 2 + h;
        double R = obj_geom->GetCylinderRadius();
        double R_grasp = R + 0.02;

        double n = 10.0;
        double angle_step = 2 * M_PI / n;

        std::vector<OpenRAVE::Vector> grasp_points;
        for (int i = 0; i < 10; i++) {
            OpenRAVE::Vector object_center(obj_pose.trans.x, obj_pose.trans.y, (h_bottom + h * i));
//      drawPoint(object_center);
            for (int j = 0; j < 10; j++) {
                OpenRAVE::Vector grasp_point(obj_pose.trans.x + R_grasp * std::cos(angle_step * j),
                                             obj_pose.trans.y + R_grasp * std::sin(angle_step * j),
                                             object_center.z);

                OpenRAVE::Vector grasp_vector = object_center - grasp_point;
                grasp_vector = grasp_vector.normalize();
//        drawPoint(grasp_point);
                drawArrow(grasp_point, grasp_vector);

            }

        }

    }
}

void FRASIEROpenRAVE::generateEEFCurve() {
    ecl::Array<double> x_set(3);
    ecl::Array<double> eef_curve(3);

    x_set << 0.0, 1.0, 2.0;
    ecl::CubicSpline curve = ecl::CubicSpline::Natural(x_set, eef_curve);
}


Grasp FRASIEROpenRAVE::generateGraspPose(const std::string &obj_name) {
    Grasp grasp;
    grasp.obj_name = obj_name;

    std::vector<OpenRAVE::KinBodyPtr> bodies;
    OpenRAVE::KinBodyPtr grasp_body;
    env_->GetBodies(bodies);
    bool object_found = false;
    for(auto body : bodies){
        if(body->GetName() == obj_name){
            grasp_body = body;
            object_found = true;
            break;
        }
    }

    if(!object_found){
        grasp.graspable = false;
        return grasp;
    }

//  OpenRAVE::Transform hsr_pose = hsr_->GetLink(base_link_)->GetTransform();

    OpenRAVE::Vector object_size = grasp_body->GetLink("base")->GetGeometry(0)->GetBoxExtents();
    OpenRAVE::Transform object_pose = grasp_body->GetTransform();

//    if (object_size.y < MAX_FINGER_APERTURE && SIDE_GRASP) {
        std::cout << "RAVE: selected side grasp for " << grasp.obj_name << std::endl;
        grasp.pose.rot = FRONT_EEF_ROT;
        grasp.pose.trans.x = object_pose.trans.x - 0.02;
        grasp.pose.trans.y = object_pose.trans.y;
        grasp.pose.trans.z = object_pose.trans.z;
        grasp.graspable = true;
//    } else if (object_size.x < MAX_FINGER_APERTURE) {
/*        std::cout << "RAVE: selected top grasp for " << grasp.obj_name << std::endl;
        grasp.pose.rot = FRONT_TOP_EEF_ROT;
        grasp.pose.trans.x = object_pose.trans.x;
        grasp.pose.trans.y = object_pose.trans.y;
        if(obj_name == "ground_Banana"){
            grasp.pose.trans.z = object_pose.trans.z + object_size[2] + 0.04;
        }
        else{
            grasp.pose.trans.z = object_pose.trans.z + object_size[2] + 0.04;
        }

        grasp.graspable = true;*/
//    } else {
//        grasp.graspable = false;
//    }

    return grasp;
}

Grasp FRASIEROpenRAVE::generateGraspPose() {
    OpenRAVE::Transform hsr_pose = hsr_->GetLink(base_link_)->GetTransform();
    Grasp grasp;
    while (true) {

        std::vector<OpenRAVE::KinBodyPtr> bodies;
        OpenRAVE::KinBodyPtr grasp_body;
        env_->GetBodies(bodies);
        double closest_distance = std::numeric_limits<double>::max();
        for (const auto body : bodies) {
            std::string body_name = body->GetName();

            if (body_name.substr(0, 8) == "tabletop") {

                OpenRAVE::Transform obj_pose = hsr_pose.inverse() * body->GetTransform();

                double distance = std::sqrt(std::pow(obj_pose.trans.x, 2) + std::pow(obj_pose.trans.y, 2));
                if (distance < closest_distance) {
                    grasp_body = body;
                    closest_distance = distance;

                }
            }

        }

        grasp.obj_name = grasp_body->GetName();

        OpenRAVE::Transform object_pose = grasp_body->GetTransform();
        OpenRAVE::Vector object_size = grasp_body->GetLink("base")->GetGeometry(0)->GetBoxExtents();

        if (object_size.y < MAX_FINGER_APERTURE) {
            std::cout << "RAVE: selected side grasp for " << grasp.obj_name << std::endl;
            //generate grasp pose within rave world frame
            OpenRAVE::Transform tmp_pose(FRONT_EEF_ROT, OpenRAVE::Vector(0,0,0));
            OpenRAVE::Transform grasp_pose_rave = hsr_pose * tmp_pose; 
            grasp.pose.rot = grasp_pose_rave.rot;
            grasp.pose.trans.x = object_pose.trans.x - 0.02;
            grasp.pose.trans.y = object_pose.trans.y;
            grasp.pose.trans.z = object_pose.trans.z;
            break;
        } else if (object_size.x < MAX_FINGER_APERTURE) {
            std::cout << "RAVE: selected top grasp for " << grasp.obj_name << std::endl;
            OpenRAVE::Transform tmp_pose(FRONT_TOP_EEF_ROT, OpenRAVE::Vector(0,0,0));
            OpenRAVE::Transform grasp_pose_rave = hsr_pose * tmp_pose; 
            grasp.pose.rot = grasp_pose_rave.rot;
            grasp.pose.trans.x = object_pose.trans.x;
            grasp.pose.trans.y = object_pose.trans.y;
            grasp.pose.trans.z = object_pose.trans.z + object_size[2] + 0.01;
            break;
        } else {
            OpenRAVE::EnvironmentMutex::scoped_lock lockenv(env_->GetMutex());
            std::string not_graspable_obj_name = "not_graspable_" + grasp.obj_name;
            grasp_body->SetName(not_graspable_obj_name);
            std::cout << "RAVE: object not graspable." << std::endl;
            continue;
        }

    }
    return grasp;

}
//Only works when the robot base is controlled by navigation stack
Grasp FRASIEROpenRAVE::generatePlacePose(std::string table_name, OpenRAVE::Vector &eef_rot) {
    Grasp release;
    OpenRAVE::Transform hsr_pose = hsr_->GetLink(base_link_)->GetTransform();
    OpenRAVE::KinBodyPtr table_body = env_->GetKinBody(table_name);
    OpenRAVE::Vector table_size = table_body->GetLink("base")->GetGeometry(0)->GetBoxExtents();

    //hardcoded release pose 0.3m+half of the table width in front of robot_base, I don't know why the table size is halved, but
    //we need to multiply it by 2 to get the correct dimension
    OpenRAVE::Transform relative_place_pose(eef_rot,
                                   OpenRAVE::Vector(0.45+table_size.x, 0, 0.25+2*table_size.z));
    
    OpenRAVE::Transform rave_place_pose = hsr_pose * relative_place_pose;
    release.obj_name = "release";
    release.pose = rave_place_pose;

    return release;

}


//OpenRAVE::Transform FRASIEROpenRAVE::generatePlacePose(std::string &obj_name) {
//    OpenRAVE::Transform place_pose;
//    place_pose.rot = FRONT_EEF_ROT;
//
//    OpenRAVE::KinBodyPtr object = env_->GetKinBody(obj_name);
//    OpenRAVE::Transform object_pose = object->GetTransform();
//    std::string shelf_name = "rack_1";
//    OpenRAVE::KinBodyPtr shelf = env_->GetKinBody(shelf_name);
//    OpenRAVE::Transform shelf_pose = shelf->GetTransform();
//
//    if (object_pose.trans.y < shelf_pose.trans.y) {
//        place_pose.trans = object_pose.trans;
//        place_pose.trans.y = place_pose.trans.y + 0.15;
//        place_pose.trans.x = place_pose.trans.x + 0.03;
//    } else {
//        place_pose.trans = object_pose.trans;
//        place_pose.trans.y = place_pose.trans.y - 0.15;
//        place_pose.trans.x = place_pose.trans.x + 0.03;
//    }
//
//    return place_pose;
//}
//
//std::vector<OpenRAVE::Transform> FRASIEROpenRAVE::generatePlacePoses() {
//    OpenRAVE::Transform hsr_pose = hsr_->GetLink(base_link_)->GetTransform();
//
//    std::vector<OpenRAVE::Transform> place_poses, mid_place_poses, left_place_poses, right_place_poses;
//
//    std::vector<OpenRAVE::KinBodyPtr> bodies;
//    env_->GetBodies(bodies);
//
//    for (const auto body : bodies) {
//        std::string body_name = body->GetName();
//
//        if (body_name.substr(0, 4) == "rack") {
//            std::cout << "RAVE: creating place poses for " << body_name << std::endl;
//
////      OpenRAVE::Transform place_pose = hsr_pose.inverse() * bodies[i]->GetTransform();
//            OpenRAVE::Transform rack_pose = body->GetTransform();
//            OpenRAVE::Transform mid_place_pose, left_place_pose, right_place_pose;
//
//            mid_place_pose.rot = FRONT_EEF_ROT;
//            mid_place_pose.trans.x = rack_pose.trans.x;
//            mid_place_pose.trans.y = rack_pose.trans.y;
//            mid_place_pose.trans.z = rack_pose.trans.z + 0.1;
//
//            mid_place_poses.push_back(mid_place_pose);
//
//            left_place_pose.rot = FRONT_EEF_ROT;
//            left_place_pose.trans.x = rack_pose.trans.x;
//            left_place_pose.trans.y = rack_pose.trans.y + 0.15;
//            left_place_pose.trans.z = rack_pose.trans.z + 0.1;
//
//            left_place_poses.push_back(left_place_pose);
//
//            right_place_pose.rot = FRONT_EEF_ROT;
//            right_place_pose.trans.x = rack_pose.trans.x;
//            right_place_pose.trans.y = rack_pose.trans.y - 0.15;
//            right_place_pose.trans.z = rack_pose.trans.z + 0.1;
//
//            right_place_poses.push_back(right_place_pose);
//
//        }
//
//    }
//
//    for (auto pose : mid_place_poses) {
//        place_poses.push_back(pose);
//    }
//
//    for (auto pose : left_place_poses) {
//        place_poses.push_back(pose);
//    }
//
//    for (auto pose : right_place_poses) {
//        place_poses.push_back(pose);
//    }
//
//    return place_poses;
//}
