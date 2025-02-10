#ifndef CCCPROXY_ACTUATORS_DRL_IMPL_H
#define CCCPROXY_ACTUATORS_DRL_IMPL_H

#include <ace/Global_Macros.h>
#include <ace/Mutex.h>
#include <dds/DCPS/Definitions.h>
#include <dds/DCPS/LocalObject.h>
#include <dds/DdsDcpsSubscriptionC.h>
#include <tao/Basic_Types.h>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#include <string>
#include "./messages.pb.h"

class ActuatorsDataReaderListenerImpl
    : public virtual OpenDDS::DCPS::LocalObject<DDS::DataReaderListener> {
 public:
  ActuatorsDataReaderListenerImpl(std::string ccc_snd_addr,
                                  std::string ccc_snd_port);
  virtual ~ActuatorsDataReaderListenerImpl(void) = default;
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
  boost::asio::ip::udp::socket* send_socket_;
  boost::asio::io_service send_service_;
  boost::asio::ip::udp::endpoint ccc_rcv_endpoint_;
};

#endif
