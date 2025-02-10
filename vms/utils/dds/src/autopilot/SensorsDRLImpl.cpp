#include "SensorsDRLImpl.h"

#include <ace/Guard_T.h>
#include <ace/Log_Msg.h>
#include <ace/OS_NS_stdlib.h>

#include <iostream>

#include "PhysicalStateC.h"
#include "PhysicalStateTypeSupportC.h"
#include "PhysicalStateTypeSupportImpl.h"

SensorsDataReaderListenerImpl::SensorsDataReaderListenerImpl() {
  PhysicalState::Sensors initial_readings;
  // don't send the initial readings to the AP controller
  this->new_readings_available_ = false;
  this->latest_readings_ = initial_readings;
}
SensorsDataReaderListenerImpl::~SensorsDataReaderListenerImpl() {}

PhysicalState::Sensors SensorsDataReaderListenerImpl::get_readings() {
  ACE_Guard<ACE_Mutex> guard(this->lock_);
  this->new_readings_available_ = false;
  return this->latest_readings_;
}

CORBA::Boolean SensorsDataReaderListenerImpl::new_readings_available() {
  ACE_Guard<ACE_Mutex> guard(this->lock_);
  return this->new_readings_available_;
}

void SensorsDataReaderListenerImpl::on_requested_deadline_missed(
    DDS::DataReader_ptr reader,
    const DDS::RequestedDeadlineMissedStatus &status) {}

void SensorsDataReaderListenerImpl::on_requested_incompatible_qos(
    DDS::DataReader_ptr reader,
    const DDS::RequestedIncompatibleQosStatus &status) {}

void SensorsDataReaderListenerImpl::on_sample_rejected(
    DDS::DataReader_ptr reader, const DDS::SampleRejectedStatus &status) {}

void SensorsDataReaderListenerImpl::on_liveliness_changed(
    DDS::DataReader_ptr reader, const DDS::LivelinessChangedStatus &status) {}

void SensorsDataReaderListenerImpl::on_subscription_matched(
    DDS::DataReader_ptr reader, const DDS::SubscriptionMatchedStatus &status) {}

void SensorsDataReaderListenerImpl::on_sample_lost(
    DDS::DataReader_ptr reader, const DDS::SampleLostStatus &status) {}

void SensorsDataReaderListenerImpl::on_data_available(
    DDS::DataReader_ptr reader) {
  PhysicalState::SensorsDataReader_var reader_i =
      PhysicalState::SensorsDataReader::_narrow(reader);

  if (!reader_i) {
    ACE_ERROR(
        (LM_ERROR,
         ACE_TEXT("ERROR: %N:%l: on_data_available() - _narrow failed!\n")));
    ACE_OS::exit(1);
  }

  PhysicalState::Sensors readings;
  DDS::SampleInfo info;

  const DDS::ReturnCode_t error = reader_i->take_next_sample(readings, info);

  if (error == DDS::RETCODE_OK) {
    if (info.valid_data) {
      ACE_Guard<ACE_Mutex> guard(this->lock_);
      // assume that every new publication is a fresh set of readings
      this->new_readings_available_ = true;
      this->latest_readings_ = readings;
    }
  } else {
    ACE_ERROR((
        LM_ERROR,
        ACE_TEXT(
            "ERROR: %N:%l: on_data_available() - take_next_sample failed!\n")));
  }
}
