#include "MissionDRLImpl.h"

#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include <dds/DdsDcpsInfrastructureC.h>
#include <tao/Basic_Types.h>

#include "AutopilotC.h"
#include "AutopilotTypeSupportC.h"

MissionDataReaderListenerImpl::MissionDataReaderListenerImpl() {
  this->mission_changed_ = false;
}

MissionDataReaderListenerImpl::~MissionDataReaderListenerImpl() {}

CORBA::Boolean MissionDataReaderListenerImpl::mission_changed() {
  ACE_Guard<ACE_Mutex> guard(this->lock_);
  return this->mission_changed_;
}

Autopilot::Mission MissionDataReaderListenerImpl::get_mission() {
  ACE_Guard<ACE_Mutex> guard(this->lock_);
  this->mission_changed_ = false;
  return this->latest_mission_;
}

void MissionDataReaderListenerImpl::on_requested_deadline_missed(
    DDS::DataReader_ptr reader,
    const DDS::RequestedDeadlineMissedStatus &status) {}

void MissionDataReaderListenerImpl::on_requested_incompatible_qos(
    DDS::DataReader_ptr reader,
    const DDS::RequestedIncompatibleQosStatus &status) {}

void MissionDataReaderListenerImpl::on_sample_rejected(
    DDS::DataReader_ptr reader, const DDS::SampleRejectedStatus &status) {}

void MissionDataReaderListenerImpl::on_liveliness_changed(
    DDS::DataReader_ptr reader, const DDS::LivelinessChangedStatus &status) {}

void MissionDataReaderListenerImpl::on_subscription_matched(
    DDS::DataReader_ptr reader, const DDS::SubscriptionMatchedStatus &status) {}

void MissionDataReaderListenerImpl::on_sample_lost(
    DDS::DataReader_ptr reader, const DDS::SampleLostStatus &status) {}

void MissionDataReaderListenerImpl::on_data_available(
    DDS::DataReader_ptr reader) {
  Autopilot::MissionDataReader_var reader_i =
      Autopilot::MissionDataReader::_narrow(reader);

  if (!reader_i) {
    ACE_ERROR(
        (LM_ERROR,
         ACE_TEXT("ERROR: %N:%l: on_data_available() - _narrow failed!\n")));
    ACE_OS::exit(1);
  }

  Autopilot::Mission mission;
  DDS::SampleInfo info;

  const DDS::ReturnCode_t error = reader_i->take_next_sample(mission, info);

  if (error == DDS::RETCODE_OK) {
    if (info.valid_data) {
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("Received a mission:\n"
                          "    id:   %i\n"
                          "    name: %s\n"
                          "    items:\n"),
                 mission.id, (const char *)(mission.name)));
    }

    CORBA::ULong mission_len = mission.mission_items.length();

    for (uint i = 0; i < mission_len; ++i) {
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("        - item %i [until completion: %i, timeout: "
                          "%i, cmd_type: %i]\n"),
                 i, mission.mission_items[i].until_completion,
                 mission.mission_items[i].timeout,
                 mission.mission_items[i].action._d()));
    }

    ACE_Guard<ACE_Mutex> guard(this->lock_);
    this->mission_changed_ = true;
    this->latest_mission_ = mission;
  } else {
    ACE_ERROR((
        LM_ERROR,
        ACE_TEXT(
            "ERROR: %N:%l: on_data_available() - take_next_sample failed!\n")));
  }
}
