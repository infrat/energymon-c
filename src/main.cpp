#include <iostream>
#include <sstream>
#include "common/platform.h"
#include "mgos_app.h"
#include "mgos_gpio.h"
#include "mgos_sys_config.h"
#include "mgos_system.h"
#include "mgos_timers.h"
#include "mgos_net.h"
#include "mgos_wifi.h"
#include "mgos_updater_common.h"
#include "mgos_mqtt.h"

int pulse_count = 0;

std::string create_topic_for_channel(int channel) {
    std::ostringstream topic;
    topic << "v1/" << mgos_sys_config_get_mqtt_user() << "/"
          << mgos_sys_config_get_mqtt_client_id() << "/data/" << channel;
    return topic.str();
}

static void pub(struct mg_connection *c, const char *fmt, ...) {
    char msg[200];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsprintf(msg, fmt, ap);
    va_end(ap);
    mg_mqtt_publish(c, create_topic_for_channel(1).c_str(), mgos_mqtt_get_packet_id(),
                    MG_MQTT_QOS(1), msg, n);
    LOG(LL_INFO, ("%s -> %s", create_topic_for_channel(1).c_str(), msg));
}

static void publish_result(void *arg) {
    /**
     * in some cases, sensors used in such applications generates two positive edges on single pulse (one for led on,
     * one for led off). it can be fixed either by adding capacitor to sensor output, or - assuming
     * each pulse generates two edges - by dividing pulse count by 2
     */
    int pulse_final = pulse_count / mgos_sys_config_get_energy_pulse_div();
    float power = ((float)pulse_final / mgos_sys_config_get_energy_pulse_per_unit())
                  / (1 / (float)(3600 / (mgos_sys_config_get_energy_upload_interval()))) * 1000;
    struct mg_connection *c = mgos_mqtt_get_global_conn();

    LOG(LL_INFO, ("Power: %f W", power));
    pub(c, "power,w=%f", power);

    pulse_count = 0;
    (void) arg;
}

static void button_cb(int pin, void *arg) {
    // nothing to do here. yet.
    (void) pin;
    (void) arg;
}

static void sensor_cb(int pin, void *arg) {
    pulse_count++;
    (void) pin;
    (void) arg;
}

void gpio_setup(void) {
    // init button
    mgos_gpio_set_button_handler(mgos_sys_config_get_energy_button_gpio(), MGOS_GPIO_PULL_UP, MGOS_GPIO_INT_EDGE_POS,
                               mgos_sys_config_get_energy_button_debounce(), button_cb, NULL);
    // init sensor
    mgos_gpio_set_button_handler(mgos_sys_config_get_energy_sensor_gpio(), MGOS_GPIO_PULL_UP, MGOS_GPIO_INT_EDGE_NEG,
                              mgos_sys_config_get_energy_sensor_debounce(), sensor_cb, NULL);
}

enum mgos_app_init_result mgos_app_init(void) {
    gpio_setup();
    mgos_set_timer(mgos_sys_config_get_energy_upload_interval() * 1000, true, publish_result, NULL);
    mgos_upd_commit();
    return MGOS_APP_INIT_SUCCESS;
}

#if 0
void loop(void) {
  /* For now, do not use delay() inside loop, use timers instead. */
}
#endif