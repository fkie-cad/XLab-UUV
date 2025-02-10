#ifndef AUTOPILOT_PROCEDUREACTIVATION_DRL_IMPL_H
#define AUTOPILOT_PROCEDUREACTIVATION_DRL_IMPL_H

#include <ace/Global_Macros.h>
#include <ace/Mutex.h>
#include <dds/DCPS/Definitions.h>
#include <dds/DCPS/LocalObject.h>
#include <dds/DdsDcpsSubscriptionC.h>
#include <tao/Basic_Types.h>

#include <vector>

#include "AutopilotC.h"

class ProcedureActivationDataReaderListenerImpl
    : public virtual OpenDDS::DCPS::LocalObject<DDS::DataReaderListener> {
 public:
  ProcedureActivationDataReaderListenerImpl();
  virtual ~ProcedureActivationDataReaderListenerImpl(void);

  std::vector<Autopilot::ProcedureActivation> get_latest_commands();

  CORBA::Boolean new_commands_available();

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
  std::vector<Autopilot::ProcedureActivation> latest_commands_;
  CORBA::Boolean command_changed_;
  ACE_Mutex lock_;
};

#endif
