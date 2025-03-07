#pragma once

#include "event_queue.hpp"
#include "libkrbn/libkrbn.h"
#include <atomic>
#include <pqrs/osx/iokit_hid_manager.hpp>
#include <pqrs/osx/iokit_hid_queue_value_monitor.hpp>

class libkrbn_hid_value_monitor final {
public:
  libkrbn_hid_value_monitor(const libkrbn_hid_value_monitor&) = delete;

  libkrbn_hid_value_monitor(libkrbn_hid_value_monitor_callback callback,
                            void* refcon) : observed_(false) {
    krbn::logger::get_logger()->info(__func__);

    std::vector<pqrs::cf::cf_ptr<CFDictionaryRef>> matching_dictionaries{
        pqrs::osx::iokit_hid_manager::make_matching_dictionary(
            pqrs::hid::usage_page::generic_desktop,
            pqrs::hid::usage::generic_desktop::keyboard),

        pqrs::osx::iokit_hid_manager::make_matching_dictionary(
            pqrs::hid::usage_page::generic_desktop,
            pqrs::hid::usage::generic_desktop::mouse),

        pqrs::osx::iokit_hid_manager::make_matching_dictionary(
            pqrs::hid::usage_page::generic_desktop,
            pqrs::hid::usage::generic_desktop::pointer),
    };

    hid_manager_ = std::make_unique<pqrs::osx::iokit_hid_manager>(pqrs::dispatcher::extra::get_shared_dispatcher(),
                                                                  matching_dictionaries);

    hid_manager_->device_matched.connect([this, callback, refcon](auto&& registry_entry_id, auto&& device_ptr) {
      if (device_ptr) {
        auto device_id = krbn::make_device_id(registry_entry_id);
        auto device_properties = std::make_shared<krbn::device_properties>(device_id,
                                                                           *device_ptr);

        auto hid_queue_value_monitor = std::make_shared<pqrs::osx::iokit_hid_queue_value_monitor>(pqrs::dispatcher::extra::get_shared_dispatcher(),
                                                                                                  *device_ptr);
        hid_queue_value_monitors_[device_id] = hid_queue_value_monitor;

        hid_queue_value_monitor->started.connect([this] {
          observed_ = true;
        });

        hid_queue_value_monitor->values_arrived.connect([this, callback, refcon, device_id](auto&& values_ptr) {
          values_arrived(callback, refcon, device_id, values_ptr);
        });

        hid_queue_value_monitor->async_start(kIOHIDOptionsTypeNone,
                                             std::chrono::milliseconds(3000));
      }
    });

    hid_manager_->device_terminated.connect([this](auto&& registry_entry_id) {
      auto device_id = krbn::make_device_id(registry_entry_id);

      hid_queue_value_monitors_.erase(device_id);
    });

    hid_manager_->error_occurred.connect([](auto&& message, auto&& kern_return) {
      krbn::logger::get_logger()->error("{0}: {1}", message, kern_return.to_string());
    });

    hid_manager_->async_start();
  }

  ~libkrbn_hid_value_monitor(void) {
    krbn::logger::get_logger()->info(__func__);

    hid_manager_ = nullptr;
    hid_queue_value_monitors_.clear();
  }

  bool get_observed(void) const {
    return observed_;
  }

private:
  void values_arrived(libkrbn_hid_value_monitor_callback callback,
                      void* refcon,
                      krbn::device_id device_id,
                      std::shared_ptr<std::vector<pqrs::cf::cf_ptr<IOHIDValueRef>>> values) {
    for (const auto& value : *values) {
      auto v = pqrs::osx::iokit_hid_value(*value);

      if (auto usage_page = v.get_usage_page()) {
        if (auto usage = v.get_usage()) {

          callback(type_safe::get(device_id),
                   type_safe::get(*usage_page),
                   type_safe::get(*usage),
                   v.get_integer_value(),
                   refcon);
        }
      }
    }
  }

  std::unique_ptr<pqrs::osx::iokit_hid_manager> hid_manager_;
  std::unordered_map<krbn::device_id, std::shared_ptr<pqrs::osx::iokit_hid_queue_value_monitor>> hid_queue_value_monitors_;
  std::atomic<bool> observed_;
};
