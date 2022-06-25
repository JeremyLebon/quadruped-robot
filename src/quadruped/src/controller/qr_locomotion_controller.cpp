// The MIT License

// Copyright (c) 2022 
// Robot Motion and Vision Laboratory at East China Normal University
// Contact: tophill.robotics@gmail.com

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "controller/qr_locomotion_controller.h"

qrLocomotionController::qrLocomotionController(qrRobot *robotIn,
                                           qrGaitGenerator *gaitGeneratorIn,
                                           qrGroundSurfaceEstimator *groundEstimatorIn,
                                           qrComPlanner *comPlannerIn,
                                           RaibertSwingLegController *swingLegControllerIn,
                                           TorqueStanceLegController *stanceLegControllerIn)
    : robot(robotIn), 
      gaitGenerator(gaitGeneratorIn),
      groundEstimator(groundEstimatorIn), 
      comPlanner(comPlannerIn), 
      swingLegController(swingLegControllerIn), 
      stanceLegController(stanceLegControllerIn)
{
    this->resetTime = robot->GetTimeSinceReset();
    this->timeSinceReset = 0.f;
}

void qrLocomotionController::Reset()
{
    this->resetTime = robot->GetTimeSinceReset();
    this->timeSinceReset = 0.;
    this->gaitGenerator->Reset(this->timeSinceReset);
    this->groundEstimator->Reset(this->timeSinceReset);
    this->comPlanner->Reset(this->timeSinceReset);
    this->swingLegController->Reset(this->timeSinceReset);
    this->stanceLegController->Reset(this->timeSinceReset);
}

void qrLocomotionController::Update()
{
    if (!this->robot->stop) { // not stop = (swingSemaphore > 0) or  (swingSemaphore=0 but not switchToSwing)
        this->timeSinceReset = this->robot->GetTimeSinceReset() - resetTime;
    }
    
    // std::cout << "-------locomotion time -------- " << timeSinceReset << std::endl;
    this->gaitGenerator->Update(this->timeSinceReset);
    
    bool switchToSwing = false;
    if (this->robot->controlParams["mode"]==LocomotionMode::WALK_LOCOMOTION) {
        // for walk mode
        const Vec4<int>& newLegState = this->gaitGenerator->legState;
        const Vec4<int>& curLegState = this->gaitGenerator->curLegState;
        for(int legId =0; legId<4; legId++) {
            if((newLegState(legId) == LegState::SWING && curLegState(legId) == LegState::STANCE)
                || newLegState(legId) == LegState::USERDEFINED_SWING) {
                    switchToSwing=true;
                    break;
                }
        }    
        if (switchToSwing) {
            if (this->swingSemaphore > 0) {
                this->swingSemaphore--;
            } else if (this->swingSemaphore == 0) 
            {
                this->swingSemaphore--;
                this->stopTick = this->robot->GetTimeSinceReset();
                this->robot->stop = true; // when stop, all legs must stay stance if phase gap=0.25.
                printf("stop robot!============\n");
                // posePlanner->ResetBasePose(this->timeSinceReset);
            } else { // swingSemaphore == -1
                ;
            }
        }
    }
    
    //
    this->groundEstimator->Update(this->timeSinceReset);
    switch (this->robot->controlParams["mode"]) {
        case LocomotionMode::POSITION_LOCOMOTION: {
            this->comPlanner->Update(this->timeSinceReset);
        } break; 
        case LocomotionMode::WALK_LOCOMOTION: {
            if (switchToSwing && this->swingSemaphore >=0) {
                // posePlanner->Update(this->timeSinceReset);
                printf("update pose plan finish\n");
            }
        } break;
        default: break;
    }
    this->swingLegController->Update(this->timeSinceReset);
    this->stanceLegController->Update(this->robot->GetTimeSinceReset() - this->resetTime);
}

std::tuple<std::vector<MotorCommand>, Eigen::Matrix<float, 3, 4>> qrLocomotionController::GetAction()
{
    this->action.clear();
    // Returns the control ouputs (e.g. positions/torques) for all motors. type: map
    auto swingAction = this->swingLegController->GetAction();
    auto [stanceAction, qpSol] = this->stanceLegController->GetAction(); // map<int, MotorCommand>
    std::vector<MotorCommand> action;
    // copy motors' actions from subcontrollers to output variable.         
    for (int joint_id = 0; joint_id < this->robot->numMotors; ++joint_id) {
        auto it = swingAction.find(joint_id);
        if (it != swingAction.end()) {
            this->action.push_back(it->second);
        } else {
            this->action.push_back(stanceAction[joint_id]);
        }
    }
    return {action, qpSol};
}

std::tuple<std::vector<MotorCommand>, Eigen::Matrix<float, 3, 4>> qrLocomotionController::GetFakeAction()
{
    this->action.clear();
    Eigen::Matrix<float, 3, 4> qpSol = Eigen::Matrix<float, 3, 4>::Zero();
    std::vector<MotorCommand> action;
    // copy motors' actions from subcontrollers to output variable.         
    for (int joint_id = 0; joint_id < this->robot->numMotors; ++joint_id) {
        this->action.push_back({0,0,0,0,0});
    }
    return {action, qpSol};
}

void qrLocomotionController::ForwardOne()
{
    this->robot->stop = false;
    this->stop = false; 
    this->swingSemaphore++;
    if (this->swingSemaphore==0) {
        this->swingSemaphore++;
        this->resetTime += (this->robot->GetTimeSinceReset() - this->stopTick);
    }
}