/* 
* Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.        
* Description: compute the force for stance controller. 
* Author: Zang Yaohua
* Create: 2021-10-25
* Notes: now this cotroller is based on floating-base simplifed dynamics, with lineard equations and balance control.
* Modify: init the file. @ Zang Yaohua;
*         classify computing forces in control frame or world frame @ Zhu Yijie 2022-02-01;
*/

#ifndef ASCEND_QUADRUPED_CPP_QP_TORQUE_OPTIMIZER_H
#define ASCEND_QUADRUPED_CPP_QP_TORQUE_OPTIMIZER_H

#include <tuple>
#include <Eigen/Dense>
#include "robots/robot.h"
#include "estimators/ground_estimator.h"

namespace Quadruped {
    Eigen::Matrix<float, 6, 12> ComputeMassMatrix(float robotMass,
                                                  Eigen::Matrix<float, 3, 3> robotInertia,
                                                  Eigen::Matrix<float, 4, 3> footPositions);
    /** @brief
     * @param float total mass of robot for MPC computing
     * @param Eigen::Matrix<int, 4, 1> 4-length array indicating whether feet is contact with ground.
     * @param float frictionCoef defines the interaction force effect between foot and env.
     * @param float min force that applys 
     * @param float max force that applys
     * @return Constraint matrix.
     */
    std::tuple<Eigen::Matrix<float, 12, 24>, Eigen::Matrix<float, 24, 1>> ComputeConstraintMatrix(
        float mpcBodyMass,
        Eigen::Matrix<bool, 4, 1> contacts,
        float frictionCoef,
        float fMinRatio,
        float fMaxRatio,
        Vec3<float> surfaceNormal={0.f,0.f,1.f});

    std::tuple<Eigen::Matrix<float, 12, 12>, Eigen::Matrix<float, 12, 1>> ComputeObjectiveMatrix(
        Eigen::Matrix<float, 6, 12> massMatrix,
        Eigen::Matrix<float, 6, 1> desiredAcc,
        Eigen::Matrix<float, 6, 1> accWeight,
        float regWeight,
        Eigen::Matrix<float, 6, 1> g);

    Eigen::Matrix<float,12,12> ComputeWeightMatrix(Robot *robot, const Eigen::Matrix<bool, 4, 1>& contacts);

    Eigen::Matrix<float, 3, 4> ComputeContactForce(Robot *robot,
                                                   GroundSurfaceEstimator* groundEstimator,
                                                   Eigen::Matrix<float, 6, 1> desiredAcc,
                                                   Eigen::Matrix<bool, 4, 1> contacts,
                                                   Eigen::Matrix<float, 6, 1> accWeight,
                                                   float regWeight=1e-4,
                                                   float frictionCoef=0.5f,
                                                   float fMinRatio=0.01f,
                                                   float fMaxRatio=10.f);
    
    Eigen::Matrix<float, 3, 4> ComputeContactForce(Robot *robot,
                                                    Eigen::Matrix<float, 6, 1> desiredAcc,
                                                    Eigen::Matrix<bool, 4, 1> contacts,
                                                    Eigen::Matrix<float, 6, 1> accWeight,
                                                    Vec3<float> normal,
                                                    Vec3<float> tangent1,
                                                    Vec3<float> tangent2,
                                                    Vec4<float> fMinRatio={0.01f,0.01f,0.01f,0.01f},
                                                    Vec4<float> fMaxRatio={10.f,10.f,10.f,10.f},
                                                    float regWeight=1e-4,
                                                    float frictionCoef=0.6f);
    
    Eigen::Matrix<float, 6, 12> ComputeMassMatrix(float robotMass,
                                                  Eigen::Matrix<float, 3, 3> robotInertia,
                                                  Eigen::Matrix<float, 4, 3> footPositions,
                                                  Mat3<float> rotMat);

    
    std::tuple<Eigen::Matrix<float, 12, 24>, Eigen::Matrix<float, 24, 1>> ComputeConstraintMatrix(
                                                                            float mpcBodyMass,
                                                                            Eigen::Matrix<bool, 4, 1> contacts,
                                                                            float frictionCoef,
                                                                            Vec4<float> fMinRatio,
                                                                            Vec4<float> fMaxRatio,
                                                                            Vec3<float> normal,
                                                                            Vec3<float> tangent1,
                                                                            Vec3<float> tangent2);
} // namespace Quadruped
#endif //ASCEND_QUADRUPED_CPP_QP_TORQUE_OPTIMIZER_H
