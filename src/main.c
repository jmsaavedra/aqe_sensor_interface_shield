/*
 * main.c
 *
 *  Created on: Jul 14, 2012
 *      Author: vic
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include "main.h"
#include "twi.h"
#include "spi.h"
#include "adc.h"
#include "egg_bus.h"
#include "digipot.h"
#include "utility.h"

/* this table stores the mapping of sensors to support hardware and associated configuration data */
static const sensor_config_t sensor_config[EGG_BUS_NUM_HOSTED_SENSORS] = {
        {NO2_HEATER_FEEDBACK_RESISTANCE, NO2_HEATER_TARGET_POWER_MW, NO2_HEATER_POWER_ADC, NO2_HEATER_FEEDBACK_ADC, DIGIPOT_WIPER0},
        {CO_HEATER_FEEDBACK_RESISTANCE, CO_HEATER_TARGET_POWER_MW, CO_HEATER_POWER_ADC, CO_HEATER_FEEDBACK_ADC, DIGIPOT_WIPER1}
};

void onRequestService(void);
void onReceiveService(uint8_t* inBytes, int numBytes);

void setup(void);

void main(void) __attribute__((noreturn));
void main(void) {
    uint8_t momentum = 1;
    int8_t last_direction = 0;

    setup();
    sei();    // enable interrupts

    // This loop runs forever, its sole purpose is to keep the heater power constant
    // it can be interrupted at any point by a TWI event
    for (;;) {
        for(uint8_t ii = 0; ii < EGG_BUS_NUM_HOSTED_SENSORS; ii++){
            int8_t direction = manageHeater(ii, momentum);
            if(direction == last_direction && direction != 0){
                momentum += 5; // change faster
            }
            else{
                momentum = 1; // reset to slow changes
            }
            last_direction = direction;
        }
        delay_sec(5); // only change the heater voltage every five seconds or so to give it time to settle in
    }
}

// this gets called when you get an SLA+R
void onRequestService(void){
    uint8_t response[4] = {0,0,0,0};
    uint16_t analog_value = 0;

    switch(egg_bus_get_command_received()){
    case EGG_BUS_COMMAND_SENSOR_COUNT:
        blinkLEDs(2, STATUS_LED); // good times
        response[3] = EGG_BUS_NUM_HOSTED_SENSORS; // this unit supports two sensors
        break;
    case EGG_BUS_COMMAND_GET_RAW_VALUE:
    case EGG_BUS_COMMAND_GET_CALCULATED_VALUE:
        blinkLEDs(1, STATUS_LED); // good times
        // read the analog sensor that has been previously commanded
        analog_value = analogRead(egg_bus_map_to_analog_pin(egg_bus_get_sensor_index_requested()));
        response[2] = uint16_low_byte(analog_value);
        response[3] = uint16_high_byte(analog_value);
        break;
    }

    // write the value back to the master per the protocol requirements
    // the response is always four bytes, most significant byte first
    twi_transmit(response, 4);
}

// this gets called when you get an SLA+W  then numBytes bytes, then stop
//   numBytes bytes have been buffered in inBytes by the twi library
// it seems quite critical that we not dilly-dally in this function, get in and get out ASAP
void onReceiveService(uint8_t* inBytes, int numBytes){
    // numBytes should always be two... per the protocol
    egg_bus_set_command_received(inBytes[0]);

    // only implement cases of commands with parameters here
    switch(inBytes[0]){
    case EGG_BUS_COMMAND_SENSOR_COUNT:
    case EGG_BUS_COMMAND_GET_CALCULATED_VALUE:
        egg_bus_set_sensor_index_requested(inBytes[1]);
        break;
    }
}

// returns -1 if the calculated power required a decrement
// returns  0 if no adjustment was needed
// returns +1 if the calculated power required an increment
int8_t manageHeater(uint8_t sensor_index, uint8_t momentum){

    sensor_config_t * scfg = &sensor_config[sensor_index];
    uint8_t power_adc_num = scfg->heater_power_adc;
    uint8_t feedback_adc_num = scfg->heater_feedback_adc;
    uint32_t feedback_resistance = scfg->heater_feedback_resistance;
    uint32_t target_power_mw = scfg->heater_target_power_mw;
    uint8_t  digipot_wiper_num = scfg->digipot_wiper;

    int8_t  return_value = 0;
    uint16_t heater_power_voltage = 0;
    uint16_t heater_feedback_voltage = 0;
    uint32_t heater_power_mw = 0;

    // read the voltage being output by the adjustable regulator
    heater_power_voltage = analogRead(power_adc_num);
    // read the voltage on the low side of the heater
    heater_feedback_voltage = analogRead(feedback_adc_num);

    // calculate the power being dissipated in the heater, integer math is fun!
    heater_power_mw = (1000L * ((((uint32_t)(heater_power_voltage - heater_feedback_voltage)) * (uint32_t) heater_feedback_voltage))) / feedback_resistance;
    // the "voltages" in this equation are actually adc counts. To get to voltage we need to multiply by 5V and divide 1024 counts (twice)
    heater_power_mw *= 5L * 5L;
    heater_power_mw /= 1024L * 1024L;
    // now we have an integer value that is actually in units of milliWatts

    // adjust the voltage being output by the adjustable regulator by
    // changing increasing or decreasing the variable feedback resistance
    if(heater_power_mw > target_power_mw){
        // cool down a bit
        digipot_decrement(digipot_wiper_num, momentum);
        return_value = -1;
    }
    else if(heater_power_mw < target_power_mw){
        // heat up a bit
        digipot_increment(digipot_wiper_num, momentum);
        return_value = 1;
    }

    return return_value;
}

void setup(void){
    POWER_LED_INIT();
    STATUS_LED_INIT();
    POWER_LED_ON();
    delay_sec(1);

    // TWI Initialize
    twi_setAddress(TWI_SLAVE_ADDRESS);
    twi_attachSlaveTxEvent(onRequestService);
    twi_attachSlaveRxEvent(onReceiveService);
    twi_init();

    // enable the adjustable regulators
    NO2_HEATER_INIT();
    CO_HEATER_INIT();
    ENABLE_NO2_HEATER();
    ENABLE_CO_HEATER();

    POWER_LED_OFF();

    spi_begin();
    digipot_init();

    blinkLEDs(2, STATUS_LED);
}

