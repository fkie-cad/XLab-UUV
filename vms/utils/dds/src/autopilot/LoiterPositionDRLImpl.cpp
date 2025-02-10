#include "LoiterPositionDRLImpl.h"

#include <ace/Log_Msg.h>
#include <ace/OS_NS_stdlib.h>
#include <dds/DdsDcpsDataReaderSeqC.h>
#include <tao/Basic_Types.h>

#include <iostream>

#include "AutopilotC.h"
#include "AutopilotTypeSupportC.h"
#include "AutopilotTypeSupportImpl.h"

LoiterPositionDataReaderListenerImpl::LoiterPositionDataReaderListenerImpl() {
  // don't send the initial position to the AP controller
  this->position_changed_ = false;
  this->latest_positions_ = std::vector<Autopilot::LoiterPosition>();
}
LoiterPositionDataReaderListenerImpl::~LoiterPositionDataReaderListenerImpl() {}

std::vector<Autopilot::LoiterPosition>
LoiterPositionDataReaderListenerImpl::get_positions() {
  ACE_Guard<ACE_Mutex> guard(this->lock_);
  this->position_changed_ = false;
  std::vector<Autopilot::LoiterPosition> r = this->latest_positions_;
  this->latest_positions_.clear();
  return r;
}

CORBA::Boolean LoiterPositionDataReaderListenerImpl::new_positions_available() {
  ACE_Guard<ACE_Mutex> guard(this->lock_);
  return this->position_changed_;
}

void LoiterPositionDataReaderListenerImpl::on_requested_deadline_missed(
    DDS::DataReader_ptr reader,
    const DDS::RequestedDeadlineMissedStatus &status) {}

void LoiterPositionDataReaderListenerImpl::on_requested_incompatible_qos(
    DDS::DataReader_ptr reader,
    const DDS::RequestedIncompatibleQosStatus &status) {}

void LoiterPositionDataReaderListenerImpl::on_sample_rejected(
    DDS::DataReader_ptr reader, const DDS::SampleRejectedStatus &status) {}

void LoiterPositionDataReaderListenerImpl::on_liveliness_changed(
    DDS::DataReader_ptr reader, const DDS::LivelinessChangedStatus &status) {}

void LoiterPositionDataReaderListenerImpl::on_subscription_matched(
    DDS::DataReader_ptr reader, const DDS::SubscriptionMatchedStatus &status) {}

void LoiterPositionDataReaderListenerImpl::on_sample_lost(
    DDS::DataReader_ptr reader, const DDS::SampleLostStatus &status) {}

void LoiterPositionDataReaderListenerImpl::on_data_available(
    DDS::DataReader_ptr reader) {
  Autopilot::LoiterPositionDataReader_var reader_i =
      Autopilot::LoiterPositionDataReader::_narrow(reader);

  if (!reader_i) {
    ACE_ERROR(
        (LM_ERROR,
         ACE_TEXT("ERROR: %N:%l: on_data_available() - _narrow failed!\n")));
    ACE_OS::exit(1);
  }

  Autopilot::LoiterPosition loiter_position;
  DDS::SampleInfo info;

  const DDS::ReturnCode_t error =
      reader_i->take_next_sample(loiter_position, info);

  if (error == DDS::RETCODE_OK) {
    if (info.valid_data) {
      ACE_Guard<ACE_Mutex> guard(this->lock_);
      this->position_changed_ = true;
      this->latest_positions_.push_back(loiter_position);
    }
  } else {
    ACE_ERROR((
        LM_ERROR,
        ACE_TEXT(
            "ERROR: %N:%l: on_data_available() - take_next_sample failed!\n")));
  }
}
