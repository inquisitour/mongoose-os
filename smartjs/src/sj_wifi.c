#include <string.h>

#include "sj_hal.h"
#include "sj_v7_ext.h"
#include "sj_wifi.h"

#include "v7/v7.h"
#include "device_config.h"

static v7_val_t s_wifi;
static struct v7 *s_v7;

static enum v7_err sj_Wifi_setup(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t ssidv = v7_arg(v7, 0);
  v7_val_t passv = v7_arg(v7, 1);
  v7_val_t extrasv = v7_arg(v7, 2);

  const char *ssid, *pass;
  size_t ssid_len, pass_len;

  if (!v7_is_string(ssidv) || !v7_is_string(passv)) {
    printf("ssid/pass are not strings\n");
    *res = v7_mk_undefined();
    goto clean;
  }

  int permanent = 1;
  if (v7_is_object(extrasv)) {
    permanent = v7_is_truthy(v7, v7_get(v7, extrasv, "permanent", ~0));
  }

  printf("permanent = %d\n", permanent);

  ssid = v7_get_string_data(v7, &ssidv, &ssid_len);
  pass = v7_get_string_data(v7, &passv, &pass_len);

  struct sys_config_wifi_sta cfg;
  cfg.ssid = (char *) ssid;
  cfg.pass = (char *) pass;
  int ret = sj_wifi_setup_sta(&cfg);
  if (ret && permanent) {
    update_sysconf(v7, "wifi.sta.enable", v7_mk_boolean(1));
    update_sysconf(v7, "wifi.sta.ssid", ssidv);
    update_sysconf(v7, "wifi.sta.pass", passv);
  }

  *res = v7_mk_boolean(ret);

  goto clean;

clean:
  return rcode;
}

static enum v7_err Wifi_connect(struct v7 *v7, v7_val_t *res) {
  (void) v7;
  *res = v7_mk_boolean(sj_wifi_connect());
  return V7_OK;
}

static enum v7_err Wifi_disconnect(struct v7 *v7, v7_val_t *res) {
  (void) v7;
  *res = v7_mk_boolean(sj_wifi_disconnect());
  return V7_OK;
}

static enum v7_err Wifi_status(struct v7 *v7, v7_val_t *res) {
  char *status = sj_wifi_get_status();
  if (status == NULL) {
    *res = v7_mk_undefined();
    goto clean;
  }
  *res = v7_mk_string(v7, status, strlen(status), 1);

clean:
  if (status != NULL) {
    free(status);
  }
  return V7_OK;
}

static enum v7_err Wifi_show(struct v7 *v7, v7_val_t *res) {
  char *ssid = sj_wifi_get_connected_ssid();
  if (ssid == NULL) {
    *res = v7_mk_undefined();
    goto clean;
  }
  *res = v7_mk_string(v7, ssid, strlen(ssid), 1);

clean:
  if (ssid != NULL) {
    free(ssid);
  }
  return V7_OK;
}

static enum v7_err Wifi_ip(struct v7 *v7, v7_val_t *res) {
  char *ip = sj_wifi_get_sta_ip();
  if (ip == NULL) {
    *res = v7_mk_undefined();
    goto clean;
  }

  *res = v7_mk_string(v7, ip, strlen(ip), 1);

clean:
  if (ip != NULL) {
    free(ip);
  }
  return V7_OK;
}

void sj_wifi_on_change_callback(enum sj_wifi_status event) {
  struct v7 *v7 = s_v7;
  v7_val_t cb = v7_get(v7, s_wifi, "_ccb", ~0);
  if (v7_is_undefined(cb) || v7_is_null(cb)) return;
  sj_invoke_cb1(s_v7, cb, v7_mk_number(event));
}

static enum v7_err Wifi_changed(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t cb = v7_arg(v7, 0);
  if (!v7_is_callable(v7, cb)) {
    *res = v7_mk_boolean(0);
    goto clean;
  }
  v7_def(v7, s_wifi, "_ccb", ~0, (V7_DESC_ENUMERABLE(0) | _V7_DESC_HIDDEN(1)),
         cb);
  *res = v7_mk_boolean(1);
  goto clean;

clean:
  return rcode;
}

void sj_wifi_scan_done(const char **ssids) {
  struct v7 *v7 = s_v7;
  v7_val_t cb = v7_get(v7, s_wifi, "_scb", ~0);
  v7_val_t res = v7_mk_undefined();
  const char **p;
  if (!v7_is_callable(v7, cb)) return;

  v7_own(v7, &res);
  if (ssids != NULL) {
    res = v7_mk_array(v7);
    for (p = ssids; *p != NULL; p++) {
      v7_array_push(v7, res, v7_mk_string(v7, *p, strlen(*p), 1));
    }
  } else {
    res = v7_mk_undefined();
  }

  sj_invoke_cb1(v7, cb, res);

  v7_disown(v7, &res);
  v7_def(v7, s_wifi, "_scb", ~0, (V7_DESC_ENUMERABLE(0) | _V7_DESC_HIDDEN(1)),
         v7_mk_undefined());
}

/* Call the callback with a list of ssids found in the air. */
static enum v7_err Wifi_scan(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  int r;
  v7_val_t cb = v7_get(v7, s_wifi, "_scb", ~0);
  if (v7_is_callable(v7, cb)) {
    fprintf(stderr, "scan in progress");
    *res = v7_mk_boolean(0);
    goto clean;
  }

  cb = v7_arg(v7, 0);
  if (!v7_is_callable(v7, cb)) {
    fprintf(stderr, "invalid argument");
    *res = v7_mk_boolean(0);
    goto clean;
  }
  v7_def(v7, s_wifi, "_scb", ~0, (V7_DESC_ENUMERABLE(0) | _V7_DESC_HIDDEN(1)),
         cb);

  r = sj_wifi_scan(sj_wifi_scan_done);
  v7_def(v7, s_wifi, "_scb", ~0, (V7_DESC_ENUMERABLE(0) | _V7_DESC_HIDDEN(1)),
         v7_mk_undefined());
  *res = v7_mk_boolean(r);
  goto clean;

clean:
  return rcode;
}

void sj_wifi_init(struct v7 *v7) {
  s_v7 = v7;
  s_wifi = v7_mk_object(v7);
  v7_set_method(v7, s_wifi, "setup", sj_Wifi_setup);
  v7_set_method(v7, s_wifi, "connect", Wifi_connect);
  v7_set_method(v7, s_wifi, "disconnect", Wifi_disconnect);
  v7_set_method(v7, s_wifi, "status", Wifi_status);
  v7_set_method(v7, s_wifi, "show", Wifi_show);
  v7_set_method(v7, s_wifi, "ip", Wifi_ip);
  v7_set_method(v7, s_wifi, "changed", Wifi_changed);
  v7_set_method(v7, s_wifi, "scan", Wifi_scan);
  v7_set(v7, v7_get_global(v7), "Wifi", ~0, s_wifi);
}
