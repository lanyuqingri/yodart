#include "BluetoothNative.h"

static JNativeInfoType this_module_native_info = {
  .free_cb = (jerry_object_native_free_callback_t)iotjs_bluetooth_destroy
};

iotjs_bluetooth_t* iotjs_bluetooth_create(const jerry_value_t jbluetooth,
                                          const jerry_char_t* name) {
  iotjs_bluetooth_t* bluetooth = IOTJS_ALLOC(iotjs_bluetooth_t);
  IOTJS_VALIDATED_STRUCT_CONSTRUCTOR(iotjs_bluetooth_t, bluetooth);
  iotjs_jobjectwrap_initialize(&_this->jobjectwrap, jbluetooth,
                               &this_module_native_info);
  _this->bt_status = -1;
  _this->bt_handle = rokidbt_create();

  int r = rokidbt_init(_this->bt_handle, (char*)name);
  if (r != 0) {
    _this->bt_status = r;
  } else {
    _this->bt_status = 0;
    rokidbt_set_event_listener(_this->bt_handle, iotjs_bluetooth_onevent,
                               (void*)bluetooth);
  }
  return bluetooth;
}

void iotjs_bluetooth_destroy(iotjs_bluetooth_t* bluetooth) {
  IOTJS_VALIDATED_STRUCT_DESTRUCTOR(iotjs_bluetooth_t, bluetooth);
  iotjs_jobjectwrap_destroy(&_this->jobjectwrap);
  if (_this->bt_handle) {
    rokidbt_destroy(_this->bt_handle);
    _this->bt_handle = NULL;
    _this->bt_status = -1;
  }
  IOTJS_RELEASE(bluetooth);
}

void iotjs_bluetooth_onevent(void* self, int what, int arg1, int arg2,
                             void* data) {
  fprintf(stdout, "got the bluetooth event %d %d %d\n", what, arg1, arg2);
  iotjs_bluetooth_t* bluetooth = (iotjs_bluetooth_t*)self;
  BluetoothEvent* event = new BluetoothEvent(bluetooth);
  event->send(what, arg1, arg2, data);
}

void BluetoothEvent::send(int what_, int arg1_, int arg2_, void* data_) {
  this->what = what_;
  this->arg1 = arg1_;
  this->arg2 = arg2_;
  this->data = data_;

  // create async handle
  uv_async_t* async = new uv_async_t;
  async->data = (void*)this;
  uv_async_init(uv_default_loop(), async, BluetoothEvent::OnCallback);
  uv_async_send(async);
}

void BluetoothEvent::OnCallback(uv_async_t* handle) {
  BluetoothEvent* event = (BluetoothEvent*)handle->data;
  iotjs_bluetooth_t* bluetooth = (iotjs_bluetooth_t*)event->handle;
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_bluetooth_t, bluetooth);

  jerry_value_t jthis = iotjs_jobjectwrap_jobject(&_this->jobjectwrap);
  jerry_value_t onevent = iotjs_jval_get_property(jthis, "onevent");
  if (!jerry_value_is_function(onevent)) {
    fprintf(stderr, "no onevent function\n");
  } else {
    uint32_t jargc = 4;
    jerry_value_t jargv[jargc] = {
      jerry_create_number((double)event->what),
      jerry_create_number((double)event->arg1),
      jerry_create_number((double)event->arg2),
    };
    if (event->data != NULL) {
      char msg[event->arg2 + 1];
      memset(msg, 0, event->arg2 + 1);
      memcpy(msg, event->data, event->arg2);
      jargv[3] = jerry_create_string_from_utf8((const jerry_char_t*)msg);
    } else {
      jargv[3] = jerry_create_null();
    }

    jerry_call_function(onevent, jerry_create_undefined(), jargv, jargc);
    for (int i = 0; i < jargc; i++) {
      jerry_release_value(jargv[i]);
    }
    jerry_release_value(onevent);
  }
  uv_close((uv_handle_t*)handle, BluetoothEvent::AfterCallback);
}

void BluetoothEvent::AfterCallback(uv_handle_t* handle) {
  BluetoothEvent* event = (BluetoothEvent*)handle->data;
  if (handle) {
    delete event;
    delete handle;
  }
}

#define IOTJS_BLUETOOTH_CHECK_INSTANCE()                               \
  do {                                                                 \
    if (_this->bt_status != 0 || _this->bt_handle == NULL) {           \
      return JS_CREATE_ERROR(COMMON, "bluetooth handle init failed."); \
    }                                                                  \
  } while (0)

JS_FUNCTION(Bluetooth) {
  DJS_CHECK_THIS();

  const jerry_value_t jbluetooth = JS_GET_THIS();
  jerry_size_t size = jerry_get_string_size(jargv[0]);
  jerry_char_t bt_name[size];
  jerry_string_to_char_buffer(jargv[0], bt_name, size);
  bt_name[size] = '\0';

  iotjs_bluetooth_t* bluetooth =
      iotjs_bluetooth_create(jbluetooth, (const jerry_char_t*)bt_name);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_bluetooth_t, bluetooth);
  IOTJS_BLUETOOTH_CHECK_INSTANCE();

  return jerry_create_undefined();
}

JS_FUNCTION(EnableBle) {
  JS_DECLARE_THIS_PTR(bluetooth, bluetooth);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_bluetooth_t, bluetooth);
  IOTJS_BLUETOOTH_CHECK_INSTANCE();

  int r = rokidbt_ble_enable(_this->bt_handle);
  if (r != 0) {
    return JS_CREATE_ERROR(COMMON, "ble start failed.");
  }
  return jerry_create_boolean(true);
}

JS_FUNCTION(DisableBle) {
  JS_DECLARE_THIS_PTR(bluetooth, bluetooth);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_bluetooth_t, bluetooth);
  IOTJS_BLUETOOTH_CHECK_INSTANCE();

  rokidbt_ble_disable(_this->bt_handle);
  return jerry_create_boolean(true);
}

JS_FUNCTION(SetName) {
  JS_DECLARE_THIS_PTR(bluetooth, bluetooth);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_bluetooth_t, bluetooth);
  IOTJS_BLUETOOTH_CHECK_INSTANCE();

  if (jerry_value_is_string(jargv[0])) {
    return JS_CREATE_ERROR(COMMON, "first argument must be a string");
  }
  jerry_size_t size = jerry_get_string_size(jargv[0]);
  jerry_char_t bt_name[size];
  jerry_string_to_char_buffer(jargv[0], bt_name, size);
  bt_name[size] = '\0';
  rokidbt_set_name(_this->bt_handle, (const char*)bt_name);
  return jerry_create_boolean(true);
}

JS_FUNCTION(BleWrite) {
  JS_DECLARE_THIS_PTR(bluetooth, bluetooth);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_bluetooth_t, bluetooth);
  IOTJS_BLUETOOTH_CHECK_INSTANCE();

  uint16_t uuid = JS_GET_ARG(0, number);
  // read the buffer
  jerry_size_t size = jerry_get_string_size(jargv[1]);
  jerry_char_t buf[size];
  jerry_string_to_char_buffer(jargv[1], buf, size);
  buf[size] = '\0';

  int r =
      rokidbt_ble_send_buf(_this->bt_handle, (uint16_t)uuid, (char*)buf, size);
  if (r != 0) {
    return JS_CREATE_ERROR(COMMON, "BLE send buffer failed.");
  } else {
    return jerry_create_boolean(true);
  }
}

JS_FUNCTION(EnableA2dp) {
  JS_DECLARE_THIS_PTR(bluetooth, bluetooth);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_bluetooth_t, bluetooth);
  IOTJS_BLUETOOTH_CHECK_INSTANCE();

  bool is_sink = JS_GET_ARG(0, boolean);
  if (is_sink) {
    rokidbt_a2dp_sink_disable(_this->bt_handle);
    if (0 != rokidbt_a2dp_sink_enable(_this->bt_handle)) {
      // a2dp sink enable failed.
      return jerry_create_boolean(false);
    }
  } else {
    if (0 != rokidbt_a2dp_enable(_this->bt_handle)) {
      // a2dp enabled failed
      return jerry_create_boolean(false);
    }
  }
  rokidbt_set_visibility(1);
  return jerry_create_boolean(true);
}

JS_FUNCTION(DisableA2dp) {
  JS_DECLARE_THIS_PTR(bluetooth, bluetooth);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_bluetooth_t, bluetooth);
  IOTJS_BLUETOOTH_CHECK_INSTANCE();

  bool is_sink = JS_GET_ARG(0, boolean);
  if (is_sink) {
    rokidbt_a2dp_sink_disable(_this->bt_handle);
  } else {
    rokidbt_a2dp_disable(_this->bt_handle);
  }
  return jerry_create_boolean(true);
}

JS_FUNCTION(CloseA2dp) {
  JS_DECLARE_THIS_PTR(bluetooth, bluetooth);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_bluetooth_t, bluetooth);
  IOTJS_BLUETOOTH_CHECK_INSTANCE();

  bool is_sink = JS_GET_ARG(0, boolean);
  if (is_sink) {
    rokidbt_a2dp_sink_close(_this->bt_handle);
  } else {
    rokidbt_a2dp_close(_this->bt_handle);
  }
  return jerry_create_boolean(true);
}

JS_FUNCTION(GetAvkState) {
  JS_DECLARE_THIS_PTR(bluetooth, bluetooth);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_bluetooth_t, bluetooth);
  IOTJS_BLUETOOTH_CHECK_INSTANCE();

  int r = rokidbt_a2dp_sink_getplay(_this->bt_handle);
  return jerry_create_boolean(r);
}

JS_FUNCTION(SendCommand) {
  JS_DECLARE_THIS_PTR(bluetooth, bluetooth);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_bluetooth_t, bluetooth);
  IOTJS_BLUETOOTH_CHECK_INSTANCE();

  int cmd = JS_GET_ARG(0, number);
  switch (cmd) {
    case A2DP_SINK_CMD_PLAY:
      rokidbt_a2dp_sink_send_play(_this->bt_handle);
      break;
    case A2DP_SINK_CMD_STOP:
      rokidbt_a2dp_sink_send_stop(_this->bt_handle);
      break;
    case A2DP_SINK_CMD_PAUSE:
      rokidbt_a2dp_sink_send_pause(_this->bt_handle);
      break;
    case A2DP_SINK_CMD_FORWARD:
      rokidbt_a2dp_sink_send_forward(_this->bt_handle);
      break;
    case A2DP_SINK_CMD_BACKWARD:
      rokidbt_a2dp_sink_send_backward(_this->bt_handle);
      break;
    default:
      fprintf(stderr, "unknown command (%d)\n", cmd);
      break;
  }
  return jerry_create_boolean(true);
}

JS_FUNCTION(SetBleVisibility) {
  JS_DECLARE_THIS_PTR(bluetooth, bluetooth);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_bluetooth_t, bluetooth);
  IOTJS_BLUETOOTH_CHECK_INSTANCE();

  bool visible = JS_GET_ARG(0, boolean);
  rokidbt_set_ble_visibility(visible);
  return jerry_create_boolean(true);
}

JS_FUNCTION(BleEnabledGetter) {
  JS_DECLARE_THIS_PTR(bluetooth, bluetooth);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_bluetooth_t, bluetooth);
  IOTJS_BLUETOOTH_CHECK_INSTANCE();

  return jerry_create_boolean(rokidbt_ble_is_enabled(_this->bt_handle));
}

void init(jerry_value_t exports) {
  jerry_value_t jconstructor = jerry_create_external_function(Bluetooth);

  /**
   * Start define constants
   */
#define IOTJS_SET_CONSTANT(jobj, name)                                    \
  do {                                                                    \
    jerry_value_t jkey = jerry_create_string((const jerry_char_t*)#name); \
    jerry_value_t jval = jerry_create_number(name);                       \
    jerry_set_property(jobj, jkey, jval);                                 \
    jerry_release_value(jkey);                                            \
    jerry_release_value(jval);                                            \
  } while (0)

  // a2dp events
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_A2DP_OPEN);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_A2DP_CLOSE);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_A2DP_START);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_A2DP_STOP);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_A2DP_RC_OPEN);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_A2DP_RC_CLOSE);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_A2DP_REMOTE_CMD);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_A2DP_REMOTE_RSP);

  // avk events
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_AVK_OPEN);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_AVK_CLOSE);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_AVK_STR_OPEN);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_AVK_STR_CLOSE);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_AVK_START);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_AVK_PAUSE);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_AVK_STOP);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_AVK_RC_OPEN);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_AVK_RC_PEER_OPEN);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_AVK_RC_CLOSE);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_AVK_SET_ABS_VOL);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_AVK_GET_PLAY_STATUS);

  // ble events
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_BLE_OPEN);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_BLE_CLOSE);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_BLE_WRITE);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_BLE_CON);

  // manager events
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_MGR_CONNECT);
  IOTJS_SET_CONSTANT(jconstructor, BT_EVENT_MGR_DISCONNECT);
#undef IOTJS_SET_CONSTANT
  /**
   * Stop constants
   */

  iotjs_jval_set_property_jval(exports, "Bluetooth", jconstructor);

  jerry_value_t proto = jerry_create_object();
  iotjs_jval_set_method(proto, "enableBle", EnableBle);
  iotjs_jval_set_method(proto, "disableBle", DisableBle);
  iotjs_jval_set_method(proto, "bleWrite", BleWrite);
  iotjs_jval_set_method(proto, "enableA2dp", EnableA2dp);
  iotjs_jval_set_method(proto, "disableA2dp", DisableA2dp);
  iotjs_jval_set_method(proto, "closeA2dp", CloseA2dp);
  iotjs_jval_set_method(proto, "getAvkState", GetAvkState);
  iotjs_jval_set_method(proto, "sendCommand", SendCommand);

  iotjs_jval_set_method(proto, "setName", SetName);
  iotjs_jval_set_method(proto, "setBleVisibility", SetBleVisibility);
  iotjs_jval_set_method(proto, "bleEnabledGetter", BleEnabledGetter);
  iotjs_jval_set_property_jval(jconstructor, "prototype", proto);

  jerry_release_value(proto);
  jerry_release_value(jconstructor);
}

NODE_MODULE(bluetooth, init)
