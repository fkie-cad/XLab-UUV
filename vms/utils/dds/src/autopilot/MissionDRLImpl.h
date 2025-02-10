#ifndef AUTOPILOT_MISSION_DRL_IMPL_H
#define AUTOPILOT_MISSION_DRL_IMPL_H

#include <ace/Mutex.h>
#include <dds/DCPS/LocalObject.h>
#include <dds/DdsDcpsSubscriptionC.h>

#include "AutopilotC.h"

class MissionDataReaderListenerImpl
    : public virtual OpenDDS::DCPS::LocalObject<DDS::DataReaderListener> {
 public:
  MissionDataReaderListenerImpl();
  virtual ~MissionDataReaderListenerImpl(void);

  Autopilot::Mission get_mission();
  CORBA::Boolean mission_changed();

  // clang-format off
  virtual void on_requested_deadline_missed(
      DDS::DataReader_ptr reader,
      const DDS::RequestedDeadlineMissedStatus& status);

  virtual void on_requested_incompatible_qos(
      DDS::DataReader_ptr reader,
      const DDS::RequestedIncompatibleQosStatus& status);

  virtual void on_sample_rejected(
      DDS::DataReader_ptr reader,
      const DDS::SampleRejectedStatus& status);

  virtual void on_liveliness_changed(
      DDS::DataReader_ptr reader,
      const DDS::LivelinessChangedStatus& status);

  virtual void on_data_available(
      DDS::DataReader_ptr reader);

  virtual void on_subscription_matched(
      DDS::DataReader_ptr reader,
      const DDS::SubscriptionMatchedStatus& status);

  virtual void on_sample_lost(
      DDS::DataReader_ptr reader,
      const DDS::SampleLostStatus& status);
  // clang-format on

 private:
  Autopilot::Mission latest_mission_;
  CORBA::Boolean mission_changed_;
  ACE_Mutex lock_;
};

#endif
