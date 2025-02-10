#include "RouteDRLImpl.h"

#include <ace/Guard_T.h>
#include <ace/Log_Msg.h>
#include <ace/Mutex.h>
#include <ace/OS_NS_stdlib.h>
#include <tao/Basic_Types.h>

#include <iostream>

#include "AutopilotC.h"
#include "AutopilotTypeSupportC.h"
#include "AutopilotTypeSupportImpl.h"

RouteDataReaderListenerImpl::RouteDataReaderListenerImpl() {
  // don't send the initial route to the AP controller
  this->route_changed_ = false;
  this->latest_routes_ = std::vector<Autopilot::Route>();
}
RouteDataReaderListenerImpl::~RouteDataReaderListenerImpl() {}

std::vector<Autopilot::Route> RouteDataReaderListenerImpl::get_routes() {
  ACE_Guard<ACE_Mutex> guard(this->lock_);
  this->route_changed_ = false;
  std::vector<Autopilot::Route> r = this->latest_routes_;
  this->latest_routes_.clear();
  return r;
}

CORBA::Boolean RouteDataReaderListenerImpl::new_routes_available() {
  ACE_Guard<ACE_Mutex> guard(this->lock_);
  return this->route_changed_;
}

void RouteDataReaderListenerImpl::on_requested_deadline_missed(
    DDS::DataReader_ptr reader,
    const DDS::RequestedDeadlineMissedStatus &status) {}

void RouteDataReaderListenerImpl::on_requested_incompatible_qos(
    DDS::DataReader_ptr reader,
    const DDS::RequestedIncompatibleQosStatus &status) {}

void RouteDataReaderListenerImpl::on_sample_rejected(
    DDS::DataReader_ptr reader, const DDS::SampleRejectedStatus &status) {}

void RouteDataReaderListenerImpl::on_liveliness_changed(
    DDS::DataReader_ptr reader, const DDS::LivelinessChangedStatus &status) {}

void RouteDataReaderListenerImpl::on_subscription_matched(
    DDS::DataReader_ptr reader, const DDS::SubscriptionMatchedStatus &status) {}

void RouteDataReaderListenerImpl::on_sample_lost(
    DDS::DataReader_ptr reader, const DDS::SampleLostStatus &status) {}

void RouteDataReaderListenerImpl::on_data_available(
    DDS::DataReader_ptr reader) {
  Autopilot::RouteDataReader_var reader_i =
      Autopilot::RouteDataReader::_narrow(reader);

  if (!reader_i) {
    ACE_ERROR(
        (LM_ERROR,
         ACE_TEXT("ERROR: %N:%l: on_data_available() - _narrow failed!\n")));
    ACE_OS::exit(1);
  }

  Autopilot::Route route;
  DDS::SampleInfo info;

  const DDS::ReturnCode_t error = reader_i->take_next_sample(route, info);

  if (error == DDS::RETCODE_OK) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("SampleInfo.sample_rank:    %i\n"
                        "SampleInfo.instance_state: %s\n"),
               info.sample_rank,
               OpenDDS::DCPS::InstanceState::instance_state_mask_string(
                   info.instance_state)
                   .c_str()));
    if (info.valid_data) {
      ACE_DEBUG((LM_DEBUG,
                 ACE_TEXT("Received a route:\n"
                          "    id:            %i\n"
                          "    name:          %s\n"
                          "    planned speed: %f\n"
                          "    waypoints:  \n"),
                 route.id, (const char *)(route.name), route.planned_speed));
      CORBA::ULong route_len = route.waypoints.length();

      for (uint i = 0; i < route_len; ++i) {
        ACE_DEBUG(
            (LM_DEBUG,
             ACE_TEXT("        - waypoint %i: [name: %s, lat: %f, lon: %f]\n"),
             i, (const char *)route.waypoints[i].name,
             route.waypoints[i].coords.latitude,
             route.waypoints[i].coords.longitude));
      }

      ACE_Guard<ACE_Mutex> guard(this->lock_);
      this->route_changed_ = true;
      this->latest_routes_.push_back(route);
    }
  } else {
    ACE_ERROR((
        LM_ERROR,
        ACE_TEXT(
            "ERROR: %N:%l: on_data_available() - take_next_sample failed!\n")));
  }
}
