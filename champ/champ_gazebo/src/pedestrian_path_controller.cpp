#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <gz/common/Console.hh>
#include <gz/math/Pose3.hh>
#include <gz/math/Vector2.hh>
#include <gz/math/Vector3.hh>
#include <gz/plugin/Register.hh>
#include <gz/sim/Entity.hh>
#include <gz/sim/EntityComponentManager.hh>
#include <gz/sim/Link.hh>
#include <gz/sim/Model.hh>
#include <gz/sim/System.hh>

namespace champ_gazebo
{
namespace
{
enum class ControllerState
{
  Normal,
  Blocked,
  Recovering,
};

double Clamp(const double _value, const double _min, const double _max)
{
  return std::max(_min, std::min(_max, _value));
}

double NormalizeAngle(double _angle)
{
  while (_angle > M_PI)
  {
    _angle -= 2.0 * M_PI;
  }
  while (_angle < -M_PI)
  {
    _angle += 2.0 * M_PI;
  }
  return _angle;
}

gz::math::Vector2d LimitVector(const gz::math::Vector2d &_value,
                               const double _maxLength)
{
  const double length = _value.Length();
  if (length <= _maxLength || length <= 1e-9)
  {
    return _value;
  }
  return _value * (_maxLength / length);
}

gz::math::Vector2d ParsePoint(const std::string &_text,
                              const gz::math::Vector2d &_fallback)
{
  std::stringstream stream(_text);
  double x = 0.0;
  double y = 0.0;
  if (stream >> x >> y)
  {
    return {x, y};
  }
  return _fallback;
}

double ReadDouble(const std::shared_ptr<const sdf::Element> &_sdf,
                  const std::string &_name,
                  const double _defaultValue)
{
  if (_sdf->HasElement(_name))
  {
    return _sdf->Get<double>(_name);
  }
  return _defaultValue;
}

const char *StateName(const ControllerState _state)
{
  switch (_state)
  {
    case ControllerState::Normal:
      return "NORMAL";
    case ControllerState::Blocked:
      return "BLOCKED";
    case ControllerState::Recovering:
      return "RECOVERING";
  }
  return "UNKNOWN";
}
}  // namespace

class PedestrianPathController
    : public ignition::gazebo::System,
      public ignition::gazebo::ISystemConfigure,
      public ignition::gazebo::ISystemPreUpdate
{
public:
  void Configure(const ignition::gazebo::Entity &_entity,
                 const std::shared_ptr<const sdf::Element> &_sdf,
                 ignition::gazebo::EntityComponentManager &_ecm,
                 ignition::gazebo::EventManager & /*_eventMgr*/) override
  {
    this->model = ignition::gazebo::Model(_entity);
    if (!this->model.Valid(_ecm))
    {
      ignerr << "PedestrianPathController must be attached to a model.\n";
      return;
    }

    if (_sdf->HasElement("link_name"))
    {
      this->linkName = _sdf->Get<std::string>("link_name");
    }
    if (_sdf->HasElement("point_a"))
    {
      this->pointA = ParsePoint(_sdf->Get<std::string>("point_a"), this->pointA);
    }
    if (_sdf->HasElement("point_b"))
    {
      this->pointB = ParsePoint(_sdf->Get<std::string>("point_b"), this->pointB);
    }

    this->cruiseSpeed = ReadDouble(_sdf, "cruise_speed", this->cruiseSpeed);
    this->maxForce = ReadDouble(_sdf, "max_force", this->maxForce);
    this->maxTorque = ReadDouble(_sdf, "max_torque", this->maxTorque);
    this->velocityGain = ReadDouble(_sdf, "velocity_gain", this->velocityGain);
    this->lateralGain = ReadDouble(_sdf, "lateral_gain", this->lateralGain);
    this->maxLateralSpeed =
        ReadDouble(_sdf, "max_lateral_speed", this->maxLateralSpeed);
    this->yawGain = ReadDouble(_sdf, "yaw_gain", this->yawGain);
    this->yawRateGain = ReadDouble(_sdf, "yaw_rate_gain", this->yawRateGain);
    this->endpointTolerance =
        ReadDouble(_sdf, "endpoint_tolerance", this->endpointTolerance);
    this->blockedSpeedThreshold =
        ReadDouble(_sdf, "blocked_speed_threshold",
                   this->blockedSpeedThreshold);
    this->blockedTimeout =
        ReadDouble(_sdf, "blocked_timeout", this->blockedTimeout);
    this->blockedForceScale =
        Clamp(ReadDouble(_sdf, "blocked_force_scale",
                         this->blockedForceScale), 0.0, 1.0);
    if (_sdf->HasElement("loop"))
    {
      this->loop = _sdf->Get<bool>("loop");
    }
    if (_sdf->HasElement("debug"))
    {
      this->debug = _sdf->Get<bool>("debug");
    }
    this->recoveringTimeout =
        ReadDouble(_sdf, "recovering_timeout", this->recoveringTimeout);
    this->recoveredSpeedThreshold =
        ReadDouble(_sdf, "recovered_speed_threshold",
                   this->recoveredSpeedThreshold);
    this->recoveryProbeInterval =
        ReadDouble(_sdf, "recovery_probe_interval",
                   this->recoveryProbeInterval);
    this->recoveryRampTime =
        ReadDouble(_sdf, "recovery_ramp_time", this->recoveryRampTime);
    this->recoveryTimeout =
        ReadDouble(_sdf, "recovery_timeout", this->recoveryTimeout);
    this->recoverySpeedThreshold =
        ReadDouble(_sdf, "recovery_speed_threshold",
                   this->recoverySpeedThreshold);
    this->statusLogPeriod =
        ReadDouble(_sdf, "status_log_period", this->statusLogPeriod);
    this->startDelay = ReadDouble(_sdf, "start_delay", this->startDelay);

    this->path = this->pointB - this->pointA;
    this->pathLength = this->path.Length();
    if (this->pathLength <= 1e-6)
    {
      ignerr << "PedestrianPathController requires distinct point_a and point_b.\n";
      return;
    }
    this->pathDir = this->path / this->pathLength;
    this->lateralDir = {-this->pathDir.Y(), this->pathDir.X()};

    const auto linkEntity = this->model.LinkByName(_ecm, this->linkName);
    if (linkEntity == ignition::gazebo::kNullEntity)
    {
      ignerr << "PedestrianPathController could not find link ["
            << this->linkName << "].\n";
      return;
    }
    this->link = ignition::gazebo::Link(linkEntity);
    this->link.EnableVelocityChecks(_ecm, true);
    this->modelName = this->model.Name(_ecm);
    this->configured = true;
    if (this->debug)
    {
      std::cerr << "PedestrianPathController configured model=["
                << this->modelName << "] link=[" << this->linkName
                << "] point_a=[" << this->pointA.X() << " "
                << this->pointA.Y() << "] point_b=[" << this->pointB.X()
                << " " << this->pointB.Y() << "] loop=[" << this->loop
                << "] max_force=[" << this->maxForce << "]" << std::endl;
    }
  }

  void PreUpdate(const ignition::gazebo::UpdateInfo &_info,
                 ignition::gazebo::EntityComponentManager &_ecm) override
  {
    if (!this->configured || _info.paused)
    {
      return;
    }
    const double simTime =
        std::chrono::duration<double>(_info.simTime).count();
    if (simTime < this->startDelay)
    {
      return;
    }

    const auto pose = this->link.WorldPose(_ecm);
    const auto linearVelocity = this->link.WorldLinearVelocity(_ecm);
    const auto angularVelocity = this->link.WorldAngularVelocity(_ecm);
    if (!pose || !linearVelocity || !angularVelocity)
    {
      return;
    }

    const gz::math::Vector2d pos{pose->Pos().X(), pose->Pos().Y()};
    const gz::math::Vector2d vel{linearVelocity->X(), linearVelocity->Y()};

    const gz::math::Vector2d target =
        this->forward ? this->pointB : this->pointA;
    const gz::math::Vector2d toTarget = target - pos;
    const double alongRemaining =
        toTarget.Dot(this->forward ? this->pathDir : -this->pathDir);
    if (alongRemaining < this->endpointTolerance)
    {
      if (this->loop)
      {
        this->forward = !this->forward;
      }
      else
      {
        this->stopped = true;
      }
    }

    const double directionSign = this->forward ? 1.0 : -1.0;
    const gz::math::Vector2d travelDir = this->pathDir * directionSign;
    const double lateralError = (this->pointA - pos).Dot(this->lateralDir);
    const double lateralSpeed =
        Clamp(this->lateralGain * lateralError,
              -this->maxLateralSpeed, this->maxLateralSpeed);

    const double alongSpeed = vel.Dot(travelDir);
    const bool commandedForward = !this->stopped && this->cruiseSpeed > 1e-6;
    const bool blockedEnabled =
        this->blockedSpeedThreshold > 0.0 && this->blockedTimeout > 0.0;
    ControllerState nextState = this->state;
    if (!blockedEnabled || !commandedForward)
    {
      this->blockedCandidate = false;
      nextState = ControllerState::Normal;
    }
    else if (this->state == ControllerState::Blocked)
    {
      if ((simTime - this->blockedStateStartTime) >=
          this->recoveryProbeInterval)
      {
        nextState = ControllerState::Recovering;
        this->recoveringStartTime = simTime;
      }
    }
    else if (this->state == ControllerState::Recovering)
    {
      const bool recovered = alongSpeed >= this->recoverySpeedThreshold;
      const bool timedOut =
          (simTime - this->recoveringStartTime) >= this->recoveryTimeout;
      if (recovered)
      {
        nextState = ControllerState::Normal;
      }
      else if (timedOut)
      {
        nextState = ControllerState::Blocked;
      }
    }
    else if (alongSpeed < this->blockedSpeedThreshold)
    {
      if (!this->blockedCandidate)
      {
        this->blockedCandidate = true;
        this->blockedStartTime = simTime;
      }
      if ((simTime - this->blockedStartTime) >= this->blockedTimeout)
      {
        nextState = ControllerState::Blocked;
      }
    }
    else
    {
      this->blockedCandidate = false;
    }

    if (nextState != this->state)
    {
      if (this->debug)
      {
        std::cerr << "PedestrianPathController [" << this->modelName << "] "
                  << StateName(this->state) << " -> " << StateName(nextState)
                  << " t=[" << simTime << "] along_speed=[" << alongSpeed
                  << "]" << std::endl;
      }
      this->state = nextState;
      if (this->state == ControllerState::Blocked)
      {
        this->blockedStateStartTime = simTime;
      }
      else if (this->state == ControllerState::Recovering)
      {
        this->recoveringStartTime = simTime;
      }
      if (this->state != ControllerState::Normal)
      {
        this->blockedCandidate = false;
      }
    }

    double forwardScale = 1.0;
    if (this->stopped)
    {
      forwardScale = 0.0;
    }
    else if (this->state == ControllerState::Blocked)
    {
      forwardScale = this->blockedForceScale;
    }
    else if (this->state == ControllerState::Recovering)
    {
      const double ratio = Clamp(
          (simTime - this->recoveringStartTime) /
          std::max(1e-6, this->recoveryRampTime), 0.0, 1.0);
      forwardScale =
          this->blockedForceScale + (1.0 - this->blockedForceScale) * ratio;
    }
    const gz::math::Vector2d desiredVelocity =
        travelDir * (this->cruiseSpeed * forwardScale) +
        this->lateralDir * (this->stopped ? 0.0 : lateralSpeed);

    gz::math::Vector2d force =
        (desiredVelocity - vel) * this->velocityGain;
    force = LimitVector(force, this->maxForce);
    const double alongForce = force.Dot(travelDir);

    const double yaw = pose->Rot().Yaw();
    const double desiredYaw = this->stopped
        ? yaw
        : std::atan2(desiredVelocity.Y(), desiredVelocity.X());
    const double yawError = NormalizeAngle(desiredYaw - yaw);
    const double yawRate = angularVelocity->Z();
    const double torqueZ =
        Clamp(this->yawGain * yawError - this->yawRateGain * yawRate,
              -this->maxTorque, this->maxTorque);

    this->link.AddWorldWrench(
        _ecm,
        {force.X(), force.Y(), 0.0},
        {0.0, 0.0, torqueZ});

    if (this->debug &&
        (simTime - this->lastStatusLogTime) >= this->statusLogPeriod)
    {
      this->lastStatusLogTime = simTime;
      std::cerr << "PedestrianPathController [" << this->modelName << "] "
                << "status t=[" << simTime << "] state=["
                << StateName(this->state) << "] x=[" << pos.X()
                << "] y=[" << pos.Y() << "] yaw=[" << yaw
                << "] along_speed=[" << alongSpeed << "] along_force=["
                << alongForce << "] forward_scale=[" << forwardScale
                << "]" << std::endl;
    }
  }

private:
  ignition::gazebo::Model model;
  ignition::gazebo::Link link;
  std::string linkName{"body_link"};
  std::string modelName;

  gz::math::Vector2d pointA{-4.0, -2.25};
  gz::math::Vector2d pointB{1.5, -2.25};
  gz::math::Vector2d path{5.5, 0.0};
  gz::math::Vector2d pathDir{1.0, 0.0};
  gz::math::Vector2d lateralDir{0.0, 1.0};
  double pathLength{5.5};

  double cruiseSpeed{0.8};
  double maxForce{8.0};
  double maxTorque{8.0};
  double velocityGain{6.0};
  double lateralGain{1.5};
  double maxLateralSpeed{0.35};
  double yawGain{6.0};
  double yawRateGain{2.0};
  double endpointTolerance{0.12};
  double blockedSpeedThreshold{0.08};
  double blockedTimeout{0.5};
  double blockedForceScale{0.2};
  double recoveringTimeout{1.0};
  double recoveredSpeedThreshold{0.25};
  double recoveryProbeInterval{1.0};
  double recoveryRampTime{1.0};
  double recoveryTimeout{2.0};
  double recoverySpeedThreshold{0.25};
  double statusLogPeriod{0.1};
  double startDelay{0.0};

  bool configured{false};
  bool debug{false};
  bool forward{true};
  bool loop{true};
  bool stopped{false};
  bool blockedCandidate{false};
  ControllerState state{ControllerState::Normal};
  double blockedStartTime{0.0};
  double blockedStateStartTime{0.0};
  double recoveringStartTime{0.0};
  double lastStatusLogTime{-1.0};
};
}  // namespace champ_gazebo

IGNITION_ADD_PLUGIN(
  champ_gazebo::PedestrianPathController,
  ignition::gazebo::System,
  champ_gazebo::PedestrianPathController::ISystemConfigure,
  champ_gazebo::PedestrianPathController::ISystemPreUpdate)

IGNITION_ADD_PLUGIN_ALIAS(
  champ_gazebo::PedestrianPathController,
  "champ_gazebo::PedestrianPathController")
