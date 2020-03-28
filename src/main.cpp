#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libusb.h>

#include <memory>
#include <string>

#include <cmdline.hpp>

std::string get_serial_for_device(libusb_device_handle *handle) {
  struct libusb_device_descriptor desc;
  int ret = libusb_get_device_descriptor(libusb_get_device(handle), &desc);
  if (ret < 0) {
    fprintf(stderr, "Failed to read device descriptor\n");
    return "";
  }

  char buf[256];
  if (desc.iSerialNumber) {
    ret = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber,
                                             reinterpret_cast<uint8_t *>(buf),
                                             sizeof(buf));
    if (ret < 0) {
      fprintf(stderr, "Failed to query serial descriptor\n");
      return "";
    }
    return std::string(buf);
  } else {
    fprintf(stderr, "Device does not have a serial number\n");
    return "";
  }
}

std::string get_serial_for_device(libusb_device *dev) {
  libusb_device_handle *handle;
  int ret = libusb_open(dev, &handle);
  if (ret < 0)
    return "";
  std::string serial = get_serial_for_device(handle);
  libusb_close(handle);
  return serial;
}

libusb_device_handle *get_device(CliArgs &args) {
  // Get a list of all the USB devices in the system
  libusb_device **devices;
  ssize_t device_count = libusb_get_device_list(nullptr, &devices);

  // Create a scoped pointer to free the list again
  std::shared_ptr<void> _defer_free_device_list(
      nullptr,
      [=](...) { // Decref and free device list
        libusb_free_device_list(devices, 1);
      });

  // Iterate the devices, and check if any of them have both the right VID:PID
  // _and_ the right serial
  for (ssize_t i = 0; i < device_count; i++) {
    // Read the descriptor for this device
    libusb_device_descriptor desc{};
    int ret = libusb_get_device_descriptor(devices[i], &desc);
    if (ret < 0) {
      fprintf(stderr, "Failed to get device descriptor\n");
      return nullptr;
    }

    // Is the VID:PID correct?
    if (desc.idVendor != args._usb_vid || desc.idProduct != args._usb_pid)
      continue;

    // Open that device up
    libusb_device_handle *handle;
    ret = libusb_open(devices[i], &handle);
    if (ret < 0) {
      fprintf(stderr, "Failed to open device\n");
      return nullptr;
    }

    // Do the arguments specify a device serial to use?
    if (!args._usb_serial_specified) {
      // Done, return the handle
      return handle;
    } else {
      // Is the serial a match?
      std::string device_serial = get_serial_for_device(handle);
      if (!args._usb_serial.compare(device_serial)) {
        return handle;
      } else {
        // Close it again, not a serial match
        libusb_close(handle);
      }
    }
  }

  // Matching device not found
  return nullptr;
}

int main(int argc, char **argv) {
  CliArgs args;
  if (!args.parse(argc, argv)) {
    return EXIT_FAILURE;
  }

  // Attempt to init libusb
  if (libusb_init(NULL) < 0) {
    fprintf(stderr, "Failed to initialize libusb\n");
    return EXIT_FAILURE;
  }

  // Try and open USB device
  libusb_device_handle *usb_handle = get_device(args);
  if (usb_handle == nullptr) {
    fprintf(stderr, "Failed to find device with VID:PID %04x:%04x\n",
            args._usb_pid, args._usb_vid);
    return EXIT_FAILURE;
  }

  // Claim programming interface
  if (libusb_claim_interface(usb_handle, args._usb_interface) < 0) {
    fprintf(stderr, "Failed to claim usb interface 0x%02x\n",
            args._usb_interface);
    return EXIT_FAILURE;
  }

  // Get the serial for this device
  std::string serial = get_serial_for_device(usb_handle);
  fprintf(stderr, "Claimed interface %u on device with serial %s\n",
          args._usb_interface, serial.c_str());

  return EXIT_SUCCESS;
}
