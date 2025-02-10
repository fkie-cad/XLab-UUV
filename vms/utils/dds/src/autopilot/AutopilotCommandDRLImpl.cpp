#include "AutopilotCommandDRLImpl.h"

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

AutopilotCommandDataReaderListenerImpl::
    AutopilotCommandDataReaderListenerImpl() {
  this->command_changed_ = false;
  this->latest_commands_ = std::vector<Autopilot::AutopilotCommand>();
}
AutopilotCommandDataReaderListenerImpl::
    ~AutopilotCommandDataReaderListenerImpl() {}

std::vector<Autopilot::AutopilotCommand>
AutopilotCommandDataReaderListenerImpl::get_latest_commands() {
  ACE_Guard<ACE_Mutex> guard(this->lock_);
  this->command_changed_ = false;
  std::vector<Autopilot::AutopilotCommand> r = this->latest_commands_;
  this->latest_commands_.clear();
  return r;
}

CORBA::Boolean
AutopilotCommandDataReaderListenerImpl::new_commands_available() {
  ACE_Guard<ACE_Mutex> guard(this->lock_);
  return this->command_changed_;
}

void AutopilotCommandDataReaderListenerImpl::on_requested_deadline_missed(
    DDS::DataReader_ptr reader,
    const DDS::RequestedDeadlineMissedStatus &status) {}

void AutopilotCommandDataReaderListenerImpl::on_requested_incompatible_qos(
    DDS::DataReader_ptr reader,
    const DDS::RequestedIncompatibleQosStatus &status) {}

void AutopilotCommandDataReaderListenerImpl::on_sample_rejected(
    DDS::DataReader_ptr reader, const DDS::SampleRejectedStatus &status) {}

void AutopilotCommandDataReaderListenerImpl::on_liveliness_changed(
    DDS::DataReader_ptr reader, const DDS::LivelinessChangedStatus &status) {}

void AutopilotCommandDataReaderListenerImpl::on_subscription_matched(
    DDS::DataReader_ptr reader, const DDS::SubscriptionMatchedStatus &status) {}

void AutopilotCommandDataReaderListenerImpl::on_sample_lost(
    DDS::DataReader_ptr reader, const DDS::SampleLostStatus &status) {}

void AutopilotCommandDataReaderListenerImpl::on_data_available(
    DDS::DataReader_ptr reader) {
  Autopilot::AutopilotCommandDataReader_var reader_i =
      Autopilot::AutopilotCommandDataReader::_narrow(reader);

  if (!reader_i) {
    ACE_ERROR(
        (LM_ERROR,
         ACE_TEXT("ERROR: %N:%l: on_data_available() - _narrow failed!\n")));
    ACE_OS::exit(1);
  }

  Autopilot::AutopilotCommand command;
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
