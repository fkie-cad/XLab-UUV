#include "DiveProcedureDRLImpl.h"

#include <ace/Log_Msg.h>
#include <ace/OS_NS_stdlib.h>
#include <dds/DdsDcpsDataReaderSeqC.h>
#include <tao/Basic_Types.h>

#include <iostream>

#include "AutopilotC.h"
#include "AutopilotTypeSupportC.h"
#include "AutopilotTypeSupportImpl.h"

DiveProcedureDataReaderListenerImpl::DiveProcedureDataReaderListenerImpl() {
  // don't send the initial position to the AP controller
  this->procedure_changed_ = false;
  this->latest_procs_ = std::vector<Autopilot::DiveProcedure>();
}
DiveProcedureDataReaderListenerImpl::~DiveProcedureDataReaderListenerImpl() {}

std::vector<Autopilot::DiveProcedure>
DiveProcedureDataReaderListenerImpl::get_procedures() {
  ACE_Guard<ACE_Mutex> guard(this->lock_);
  this->procedure_changed_ = false;
  std::vector<Autopilot::DiveProcedure> r = this->latest_procs_;
  this->latest_procs_.clear();
  return r;
}

CORBA::Boolean DiveProcedureDataReaderListenerImpl::new_procs_available() {
  ACE_Guard<ACE_Mutex> guard(this->lock_);
  return this->procedure_changed_;
}

void DiveProcedureDataReaderListenerImpl::on_requested_deadline_missed(
    DDS::DataReader_ptr reader,
    const DDS::RequestedDeadlineMissedStatus &status) {}

void DiveProcedureDataReaderListenerImpl::on_requested_incompatible_qos(
    DDS::DataReader_ptr reader,
    const DDS::RequestedIncompatibleQosStatus &status) {}

void DiveProcedureDataReaderListenerImpl::on_sample_rejected(
    DDS::DataReader_ptr reader, const DDS::SampleRejectedStatus &status) {}

void DiveProcedureDataReaderListenerImpl::on_liveliness_changed(
    DDS::DataReader_ptr reader, const DDS::LivelinessChangedStatus &status) {}

void DiveProcedureDataReaderListenerImpl::on_subscription_matched(
    DDS::DataReader_ptr reader, const DDS::SubscriptionMatchedStatus &status) {}

void DiveProcedureDataReaderListenerImpl::on_sample_lost(
    DDS::DataReader_ptr reader, const DDS::SampleLostStatus &status) {}

void DiveProcedureDataReaderListenerImpl::on_data_available(
    DDS::DataReader_ptr reader) {
  Autopilot::DiveProcedureDataReader_var reader_i =
      Autopilot::DiveProcedureDataReader::_narrow(reader);

  if (!reader_i) {
    ACE_ERROR(
        (LM_ERROR,
         ACE_TEXT("ERROR: %N:%l: on_data_available() - _narrow failed!\n")));
    ACE_OS::exit(1);
  }

  Autopilot::DiveProcedure loiter_position;
  DDS::SampleInfo info;

  const DDS::ReturnCode_t error =
      reader_i->take_next_sample(loiter_position, info);

  if (error == DDS::RETCODE_OK) {
    if (info.valid_data) {
      ACE_Guard<ACE_Mutex> guard(this->lock_);
      this->procedure_changed_ = true;
      this->latest_procs_.push_back(loiter_position);
    }
  } else {
    ACE_ERROR((
        LM_ERROR,
        ACE_TEXT(
            "ERROR: %N:%l: on_data_available() - take_next_sample failed!\n")));
  }
}
