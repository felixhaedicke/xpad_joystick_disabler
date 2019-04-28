/*
 * xpad_joystick_disabler
 * Helper program to disable other joysticks when an Xpad device is active
 *
 * Licensed under the GNU GENERAL PUBLIC LICENSE Version 2.
 * See LICENSE for details.
 */

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <libudev.h>


#define SYSFS_HID_GENERIC_BIND    "/sys/bus/hid/drivers/hid-generic/bind"
#define SYSFS_HID_GENERIC_UNBIND  "/sys/bus/hid/drivers/hid-generic/unbind"


enum handle_js_parent_ret { IGNORE_PARENT, NEXT_DEVICE, CANCEL_ENUMERATION };

typedef enum handle_js_parent_ret (*handle_js_parent_device_cb)(
    struct udev_device* parent,
    void* user_data);

static void enumerate_js_parent_devices(struct udev* udev,
                                        handle_js_parent_device_cb cb,
                                        void* cb_user_data)
{
  assert(udev);
  assert(cb);

  struct udev_enumerate* enumerate = udev_enumerate_new(udev);
  assert(enumerate);

  int status = udev_enumerate_add_match_subsystem(enumerate, "input");
  assert(0 <= status);

  status = udev_enumerate_scan_devices(enumerate);
  if (0 <= status)
  {
    struct udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);
    if (devices)
    {
      struct udev_list_entry* entry;
      bool cancel = false;
      udev_list_entry_foreach(entry, devices)
      {
        const char* path = udev_list_entry_get_name(entry);
        if (path)
        {
          struct udev_device* dev = udev_device_new_from_syspath(udev, path);
          if (dev)
          {
            const char* sysname = udev_device_get_sysname(dev);

            if (sysname && ('j' == sysname[0]) && ('s' == sysname[1]))
            {
              struct udev_device* parent = udev_device_get_parent(dev);
              bool next_device = false;
              while (parent && !next_device &&!cancel)
              {
                switch (cb(parent, cb_user_data))
                {
                  case IGNORE_PARENT:
                    break;

                  case NEXT_DEVICE:
                    next_device = true;
                    break;

                  case CANCEL_ENUMERATION:
                    cancel = true;
                    break;
                }

                parent = udev_device_get_parent(parent);
              }

              udev_device_unref(dev);
            }
          }
        }

        if (cancel) break;
      }
    }
  }

  udev_enumerate_unref(enumerate);
}

static void write_str_to_file(const char* path, const char* value)
{
  assert(path);
  assert(value);

  int fd = open(path, O_WRONLY);
  assert(0 <= fd);

  write(fd, value, strlen(value));

  close(fd);
}

static enum handle_js_parent_ret check_for_xpad_cb(
    struct udev_device* parent,
    void* user_data)
{
  assert(parent);
  assert(user_data);

  const char* parent_driver = udev_device_get_driver(parent);
  if ((parent_driver) && (0 == strcmp("xpad", parent_driver)))
  {
    *((bool*) user_data) = true;
    return CANCEL_ENUMERATION;
  }
  else
  {
    return IGNORE_PARENT;
  }
}

static bool check_if_xpad_active(struct udev* udev)
{
  assert(udev);

  // check if there is a js<X> device with the "xpad" driver as parent

  bool have_xpad = false;
  enumerate_js_parent_devices(udev, check_for_xpad_cb, &have_xpad);
  return have_xpad;
}

static enum handle_js_parent_ret deactivate_device_if_hid_generic_cb(
    struct udev_device* parent,
    void* user_data)
{
  assert(parent);

  const char* parent_driver = udev_device_get_driver(parent);
  if ((parent_driver) && (0 == strcmp("hid-generic", parent_driver)))
  {
    const char* parent_sysname = udev_device_get_sysname(parent);
    if (parent_sysname)
    {
      write_str_to_file(SYSFS_HID_GENERIC_UNBIND, parent_sysname);
      return NEXT_DEVICE;
    }
  }

  return IGNORE_PARENT;
}

static void deactivate_hid_generic_js_devices(struct udev* udev)
{
  assert(udev);

  // if there is a js<X> device with the "hid-generic" driver as parent,
  // deactivate (unbind) it.
  enumerate_js_parent_devices(udev, deactivate_device_if_hid_generic_cb, NULL);
}

static void activate_hid_generic_js_devices(struct udev* udev)
{
  assert(udev);

  // enumerate devices in the "hid" subsystem.
  struct udev_enumerate* enumerate = udev_enumerate_new(udev);
  assert(enumerate);

  int status = udev_enumerate_add_match_subsystem(enumerate, "hid");
  assert(0 <= status);

  status = udev_enumerate_scan_devices(enumerate);
  if (0 <= status)
  {
    struct udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);
    if (devices)
    {
      struct udev_list_entry* entry;
      udev_list_entry_foreach(entry, devices)
      {
        const char* path = udev_list_entry_get_name(entry);
        if (path)
        {
          struct udev_device* dev = udev_device_new_from_syspath(udev, path);
          if (dev)
          {
            const char* driver = udev_device_get_driver(dev);
            // this HID device has no driver assigned - try to bind to
            // the hid-generic driver.
            if (!driver)
            {
              const char* sysname = udev_device_get_sysname(dev);
              if (sysname)
              {
                write_str_to_file(SYSFS_HID_GENERIC_BIND, sysname);
              }
            }

            udev_device_unref(dev);
          }
        }
      }
    }
  }
}

int main(int argc, char* argv[])
{
  struct udev* udev = udev_new();
  if (!udev)
  {
    fprintf(stderr, "Could not acquire udev context");
    return 1;
  }

  if (check_if_xpad_active(udev))
  {
    printf("Xpad device active - deactivate other joystick devices\n");
    deactivate_hid_generic_js_devices(udev);
  }
  else
  {
    printf("No Xpad device active - activate all joystick devices\n");
    activate_hid_generic_js_devices(udev);
  }

  udev_unref(udev);
  return 0;
}
