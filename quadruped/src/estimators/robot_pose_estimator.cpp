/*
* Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
* Description: estimate the vel of quadruped.
* Author: Zhu Yijie
* Create: 2021-11-25
* Notes: xx
* Modify: init the file. @ Zhu Yijie
*         add simulation position case. @ Zhu Yijie 2022.02.01
*/

#include "estimators/robot_pose_estimator.h"

namespace Quadruped {

    RobotPoseEstimator::RobotPoseEstimator(Robot *robotIn,
                                           GaitGenerator *gaitGeneratorIn,
                                           GroundSurfaceEstimator *groundEstimatorIn,
                                           RobotVelocityEstimator *velocityEstimatorIn)
        : robot(robotIn), gaitGenerator(gaitGeneratorIn), groundEstimator(groundEstimatorIn), velocityEstimator(velocityEstimatorIn)
    {
        lastTimestamp = 0.f;
        estimatedPose << robot->GetBasePosition(), robot->GetBaseRollPitchYaw();
    }

    void RobotPoseEstimator::Reset(float currentTime)
    {
        lastTimestamp = 0.f;
        estimatedPose << robot->GetBasePosition(), robot->GetBaseRollPitchYaw();
        robot->stateDataFlow.heightInControlFrame = robot->bodyHeight;
    }

    float RobotPoseEstimator::ComputeDeltaTime(const LowState *robotState)
    {
        float deltaTime;
        if (std::abs(lastTimestamp) < 1e-5) {
            // First timestamp received, return an estimated delta_time.
            deltaTime = robot->timeStep;
        } else {
            deltaTime = (robotState->tick - lastTimestamp) / 1000.;
        }
        lastTimestamp = robotState->tick;
        return deltaTime;
    }

    void RobotPoseEstimator::Update(float currentTime)
    {
        const LowState &robotState = robot->lowState;
        // Propagate current state estimate with new accelerometer reading.
        float deltaTime = ComputeDeltaTime(&robotState);
        float height = EstimateRobotHeight();
        estimatedPose[2] = height;
        robot->basePosition[2] = height;
        ComputePose(deltaTime);
        
        // case 2 : in simulation case, // todo
        if (robot->isSim) {
            estimatedPose[0] = robot->gazeboBasePosition[0];
            estimatedPose[1] = robot->gazeboBasePosition[1];
            // estimatedPose[2] = robot->gazeboBasePosition[2];
            robot->basePosition[0] = estimatedPose[0];
            robot->basePosition[1] = estimatedPose[1];
            // robot->basePosition[2] = estimatedPose[2];
        }
    }

    float RobotPoseEstimator::EstimateRobotHeight()
    {
        Quat<float> baseOrientation;
        Eigen::Matrix<float, 3, 3> rotMat;
        Eigen::Matrix<float, 3, 4> footPositions;
        Eigen::Matrix<float, 3, 4> footPositionsWorldFrame, footPositionsControlFrame;
        Vec4<float> usefulHeights;

        Eigen::Matrix<int, 1, 4> contacts;
        for (int legId = 0; legId < 4; legId++) {
            int desLegState = gaitGenerator->desiredLegState[legId];
            int legState = gaitGenerator->detectedLegState[legId];
            bool flag;
            if (robot->controlParams["mode"] == LocomotionMode::WALK_LOCOMOTION) {
                flag = (desLegState!=SubLegState::TRUE_SWING 
                        || desLegState == LegState::STANCE
                        || (desLegState==SubLegState::TRUE_SWING && legState == LegState::EARLY_CONTACT));
            } else {
                flag = (desLegState == LegState::STANCE);
            }
            // if (flag && robot->footContact[legId]) {
            if (flag) {
                contacts[legId] = true;
            } else {
                contacts[legId] = false;
            }
        }

        if (contacts.sum() == 0) {
            // All foot in air, no way to estimate
            return robot->bodyHeight;
        } else {
            baseOrientation = robot->GetBaseOrientation();
            rotMat = robot->stateDataFlow.baseRMat;
            footPositions = robot->GetFootPositionsInBaseFrame();
            footPositionsWorldFrame = rotMat * footPositions;
            Mat3<float> groundOrientationMat = groundEstimator->GetAlignedDirections();
            footPositionsControlFrame = groundOrientationMat.transpose() * footPositionsWorldFrame;
            Vec4<float> heightsInControlFrame = -footPositionsControlFrame.block<1, 4>(2, 0).cwiseProduct(contacts.cast<float>());
            robot->stateDataFlow.heightInControlFrame = heightsInControlFrame.sum() / contacts.sum();
            usefulHeights = -footPositionsWorldFrame.block<1, 4>(2, 0).cwiseProduct(contacts.cast<float>());
            return usefulHeights.sum() / contacts.sum();
        }
    }

    void RobotPoseEstimator::ComputePose(float deltaTime)
    {
        // currentTime = 0;//ros::Time::now();
        const Vec3<float> &estimatedVelocity = velocityEstimator->GetEstimatedVelocity();
        const Vec3<float> &baseRollPitchYawRate = velocityEstimator->GetEstimatedAngularVelocity();
        const Vec3<float> &baseRollPitchYaw = robot->GetBaseRollPitchYaw();
        float vX = estimatedVelocity[0];
        float vY = estimatedVelocity[1];
        float vZ = estimatedVelocity[2];
        float vTheta = baseRollPitchYawRate[2];
        float x = estimatedPose[0];
        float y = estimatedPose[1];
        float theta = estimatedPose[5];
        // compute odometry in a typical way given the velocities of the robot
        float deltaT = deltaTime; //(currentTime - lastTime).toSec();
        float deltaX = (vX * cos(theta) - vY * sin(theta)) * deltaT;
        float deltaY = (vX * sin(theta) + vY * cos(theta)) * deltaT;
        float deltaTheta = vTheta * deltaT;

        x += deltaX * 1.1f; // sensor error, 1.1 is empirical factor
        y += deltaY * 1.1f;
        robot->absolutHight += vZ*deltaT;
        theta += deltaTheta;
        // update robot position
        estimatedPose[0] = x;
        estimatedPose[1] = y;
        estimatedPose[5] = theta;

        robot->basePosition[0] = x;
        robot->basePosition[1] = y;
    }
} // Quadruped
