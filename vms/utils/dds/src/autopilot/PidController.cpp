#include "PidController.h"

#include <ace/Log_Msg.h>
#include <ace/Log_Priority.h>
#include <ace/OS_NS_time.h>
#include <ace/ace_wchar.h>

PidController::PidController(CORBA::Double kp, CORBA::Double ki,
                             CORBA::Double kd)
    : PidController(kp, ki, kd, 0.0, 1.0, 1.0) {}

PidController::PidController(CORBA::Double kp, CORBA::Double ki,
                             CORBA::Double kd, CORBA::Double min,
                             CORBA::Double max)
    : PidController(kp, ki, kd, min, max, 1.0) {}

PidController::PidController(CORBA::Double kp, CORBA::Double ki,
                             CORBA::Double kd, CORBA::Double min,
                             CORBA::Double max, CORBA::Double integral_decay)
    : kp_{kp},
      ki_{ki},
      kd_{kd},
      min_{min},
      max_{max},
      integral_decay_{integral_decay} {
  this->integral_ = 0.0;
  this->previous_error_ = 0.0;
  this->last_ts_ = 0.0;
}

CORBA::Double PidController::compute_error(CORBA::Double measured,
                                           CORBA::Double setpoint) {
  return setpoint - measured;
}

CORBA::Double AngularPidController::compute_error(CORBA::Double measured,
                                                  CORBA::Double setpoint) {
  CORBA::Double error = setpoint - measured;
  if (error < -180.0) error += 360.0;
  if (error > 180.0) error -= 360.0;
  return error;
}

CORBA::Double PidController::control(CORBA::Double measured,
                                     CORBA::Double setpoint) {
  CORBA::Double error = this->compute_error(measured, setpoint);
  ACE_hrtime_t now = ACE_OS::gethrtime();
  // convert from ns to seconds
  CORBA::Double delta = (now - this->last_ts_) * 1e-9;

  // ignore integral and derivative on first iteration
  if (this->last_ts_ == 0.0) delta = 0.0;
  // reset integral and ignore derivative after a timeout
  if (delta > this->TIMEOUT) {
    delta = 0.0;
    this->integral_ = 0.0;
  }
  this->last_ts_ = now;

  CORBA::Double derivative = (error - this->previous_error_) * delta;
  this->previous_error_ = error;

  // decay the integral to reduce potential windup in long scenarios
  // this is probably not always desirable though, so prefer using
  // integral_decay=1.0 if it works
  this->integral_ = this->integral_ * this->integral_decay_ + (error * delta);

  CORBA::Double output =
      this->kp_ * error + this->ki_ * this->integral_ + this->kd_ * derivative;

  // clamp output
  CORBA::Boolean saturated = false;
  if (output > this->max_) {
    output = this->max_;
    saturated = true;
  } else if (output < this->min_) {
    output = this->min_;
    saturated = true;
  }

  // avoid causing windup through process saturation
  if (saturated) this->integral_ -= (error * delta);

  ACE_DEBUG((LM_DEBUG,
             ACE_TEXT("PID control:\n"
                      "    target:     %f\n"
                      "    current:    %f\n"
                      "    delta:      %f\n"
                      "    error:      %f\n"
                      "    integral:   %f\n"
                      "    derivative: %f\n"
                      "    output:     %f\n"),
             setpoint, measured, delta, error, this->integral_, derivative,
             output));

  return output;
}
