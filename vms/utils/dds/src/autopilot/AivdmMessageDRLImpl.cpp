#include "AivdmMessageDRLImpl.h"

#include "PhysicalStateTypeSupportC.h"

AivdmMessageDataReaderListenerImpl::AivdmMessageDataReaderListenerImpl() {
  this->new_messages_available_ = false;
  this->latest_messages_ = std::vector<PhysicalState::AivdmMessage>();
}

std::vector<PhysicalState::AivdmMessage>
AivdmMessageDataReaderListenerImpl::get_messages() {
  ACE_Guard<ACE_Mutex> guard(this->lock_);
  this->new_messages_available_ = false;
  std::vector<PhysicalState::AivdmMessage> r = this->latest_messages_;
  this->latest_messages_.clear();
  return r;
}

CORBA::Boolean AivdmMessageDataReaderListenerImpl::new_messages_available() {
  ACE_Guard<ACE_Mutex> guard(this->lock_);
  return this->new_messages_available_;
}

void AivdmMessageDataReaderListenerImpl::on_requested_deadline_missed(
    DDS::DataReader_ptr reader,
    const DDS::RequestedDeadlineMissedStatus &status) {}

void AivdmMessageDataReaderListenerImpl::on_requested_incompatible_qos(
    DDS::DataReader_ptr reader,
    const DDS::RequestedIncompatibleQosStatus &status) {}

void AivdmMessageDataReaderListenerImpl::on_sample_rejected(
    DDS::DataReader_ptr reader, const DDS::SampleRejectedStatus &status) {}

void AivdmMessageDataReaderListenerImpl::on_liveliness_changed(
    DDS::DataReader_ptr reader, const DDS::LivelinessChangedStatus &status) {}

void AivdmMessageDataReaderListenerImpl::on_subscription_matched(
    DDS::DataReader_ptr reader, const DDS::SubscriptionMatchedStatus &status) {}

void AivdmMessageDataReaderListenerImpl::on_sample_lost(
    DDS::DataReader_ptr reader, const DDS::SampleLostStatus &status) {}

void AivdmMessageDataReaderListenerImpl::on_data_available(
    DDS::DataReader_ptr reader) {
  PhysicalState::AivdmMessageDataReader_var reader_i =
      PhysicalState::AivdmMessageDataReader::_narrow(reader);

  if (!reader_i) {
    ACE_ERROR(
        (LM_ERROR,
         ACE_TEXT("ERROR: %N:%l: on_data_available() - _narrow failed!\n")));
    ACE_OS::exit(1);
  }

  PhysicalState::AivdmMessage ais_message;
  DDS::SampleInfo info;

  const DDS::ReturnCode_t error = reader_i->take_next_sample(ais_message, info);

  // proxy for BC, forward message to CCC
  if (error == DDS::RETCODE_OK) {
    if (info.valid_data) {
      ACE_Guard<ACE_Mutex> guard(this->lock_);
      this->new_messages_available_ = true;
      this->latest_messages_.push_back(ais_message);
    }
  } else {
    ACE_ERROR((
        LM_ERROR,
        ACE_TEXT(
            "ERROR: %N:%l: on_data_available() - take_next_sample failed!\n")));
  }
}
