#ifndef AUTOPILOT_LOITERPOSITION_DRL_IMPL_H
#define AUTOPILOT_LOITERPOSITION_DRL_IMPL_H

#include <ace/Global_Macros.h>
#include <ace/Mutex.h>
#include <dds/DCPS/Definitions.h>
#include <dds/DCPS/LocalObject.h>
#include <dds/DdsDcpsSubscriptionC.h>
#include <tao/Basic_Types.h>

#include <vector>

#include "AutopilotC.h"

class LoiterPositionDataReaderListenerImpl
    : public virtual OpenDDS::DCPS::LocalObject<DDS::DataReaderListener> {
 public:
  LoiterPositionDataReaderListenerImpl();
  virtual ~LoiterPositionDataReaderListenerImpl(void);

  std::vector<Autopilot::LoiterPosition> get_positions();
  CORBA::Boolean new_positions_available();

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
  std::vector<Autopilot::LoiterPosition> latest_positions_;
  CORBA::Boolean position_changed_;
  ACE_Mutex lock_;
};

#endif
