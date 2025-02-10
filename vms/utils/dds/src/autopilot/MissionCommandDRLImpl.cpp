#include "MissionCommandDRLImpl.h"

#include <ace/Guard_T.h>
#include <ace/Log_Msg.h>
#include <ace/Mutex.h>
#include <ace/OS_NS_stdlib.h>
#include <dds/DdsDcpsInfrastructureC.h>

#include <iostream>
#include <vector>

#include "AutopilotC.h"
#include "AutopilotTypeSupportC.h"
#include "AutopilotTypeSupportImpl.h"

MissionCommandDataReaderListenerImpl::MissionCommandDataReaderListenerImpl() {
  this->command_changed_ = false;
  this->latest_commands_ = std::vector<Autopilot::MissionCommand>();
}
MissionCommandDataReaderListenerImpl::~MissionCommandDataReaderListenerImpl() {}

std::vector<Autopilot::MissionCommand>
MissionCommandDataReaderListenerImpl::get_latest_commands() {
  ACE_Guard<ACE_Mutex> guard(this->lock_);
  this->command_changed_ = false;
  std::vector<Autopilot::MissionCommand> r = this->latest_commands_;
  this->latest_commands_.clear();
  return r;
}

CORBA::Boolean MissionCommandDataReaderListenerImpl::new_commands_available() {
  ACE_Guard<ACE_Mutex> guard(this->lock_);
  return this->command_changed_;
}

void MissionCommandDataReaderListenerImpl::on_requested_deadline_missed(
    DDS::DataReader_ptr reader,
    const DDS::RequestedDeadlineMissedStatus &status) {}

void MissionCommandDataReaderListenerImpl::on_requested_incompatible_qos(
    DDS::DataReader_ptr reader,
    const DDS::RequestedIncompatibleQosStatus &status) {}

void MissionCommandDataReaderListenerImpl::on_sample_rejected(
    DDS::DataReader_ptr reader, const DDS::SampleRejectedStatus &status) {}

void MissionCommandDataReaderListenerImpl::on_liveliness_changed(
    DDS::DataReader_ptr reader, const DDS::LivelinessChangedStatus &status) {}

void MissionCommandDataReaderListenerImpl::on_subscription_matched(
    DDS::DataReader_ptr reader, const DDS::SubscriptionMatchedStatus &status) {}

void MissionCommandDataReaderListenerImpl::on_sample_lost(
    DDS::DataReader_ptr reader, const DDS::SampleLostStatus &status) {}

void MissionCommandDataReaderListenerImpl::on_data_available(
    DDS::DataReader_ptr reader) {
  Autopilot::MissionCommandDataReader_var reader_i =
      Autopilot::MissionCommandDataReader::_narrow(reader);

  if (!reader_i) {
    ACE_ERROR(
        (LM_ERROR,
         ACE_TEXT("ERROR: %N:%l: on_data_available() - _narrow failed!\n")));
    ACE_OS::exit(1);
  }

  Autopilot::MissionCommand command;
  DDS::SampleInfo info;

  const DDS::ReturnCode_t error = reader_i->take_next_sample(command, info);

  if (error == DDS::RETCODE_OK) {
    if (info.valid_data) {
      ACE_Guard<ACE_Mutex> guard(this->lock_);
      this->command_changed_ = true;
      this->latest_commands_.push_back(command);
    }
  } else {
    ACE_ERROR((
        LM_ERROR,
        ACE_TEXT(
            "ERROR: %N:%l: on_data_available() - take_next_sample failed!\n")));
  }
}
