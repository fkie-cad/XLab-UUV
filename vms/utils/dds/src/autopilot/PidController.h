#ifndef PIDCONTROLLER_H
#define PIDCONTROLLER_H

#include <tao/Basic_Types.h>

class PidController {
 public:
  PidController(CORBA::Double kp, CORBA::Double ki, CORBA::Double kd);
  PidController(CORBA::Double kp, CORBA::Double ki, CORBA::Double kd,
                CORBA::Double min, CORBA::Double max);
  PidController(CORBA::Double kp, CORBA::Double ki, CORBA::Double kd,
                CORBA::Double min, CORBA::Double max,
                CORBA::Double integral_decay);

  CORBA::Double control(CORBA::Double measured, CORBA::Double setpoint);

 private:
  virtual CORBA::Double compute_error(CORBA::Double measured,
                                      CORBA::Double setpoint);

  const CORBA::Double TIMEOUT = 15.0;
  CORBA::Double kp_;
  CORBA::Double ki_;
  CORBA::Double kd_;

  CORBA::Double min_;
  CORBA::Double max_;

  CORBA::Double integral_decay_;

  CORBA::Double integral_;
  CORBA::Double previous_error_;
  CORBA::Double last_ts_;
};

class AngularPidController : public PidController {
 public:
  using PidController::PidController;

 private:
  CORBA::Double compute_error(CORBA::Double measured, CORBA::Double setpoint);
};

#endif
