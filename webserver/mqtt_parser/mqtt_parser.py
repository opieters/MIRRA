# -*- coding: utf-8 -*-
"""
    *************
    __init__.py
    *************

    This file contains the logic for the MQTT client of the application. It implements all the MQTT
    event handlers and connects to the MQTT server and subscribes to the subject as defined in the
    config file.
"""

import paho.mqtt.client as mqtt
import struct
import config
import math

from mysql_manager import mysql_manager
from utils import debug_print, get_current_timestamp, convert_epoch_to_mysql_timestamp

mysql_manager = mysql_manager(config.db_settings['host'],
                              config.db_settings['port'],
                              config.db_settings['database_name'],
                              config.db_settings['username'],
                              config.db_settings['password'])


# the callback to be executed when connected to the broker.
def on_connect(client, userdata, flags, rc):
    client.subscribe(config.mqqt_settings['prefix'])
    debug_print("[MQTT] Connected to broker with prefix " + config.mqqt_settings['prefix'] + ". Result code: " + str(rc))


def on_message(client, userdata, msg):
    """
    Executed when an MQTT message is received. 

    Message format:
    [timestamp 4]:[n readouts 1]:[readout1 5]:...[readoutn 5]

    Each readout has the following format:
    [sensor_id 1]:[data 4 (float)]
    """
    debug_print("[MQTT] message received " + str(msg.topic) + " -- " + str(msg.payload))
    hierarchy = str(msg.topic).split('/')
    payload = msg.payload

    gateway_uuid = hierarchy[1]
    module_uuid = hierarchy[2]

    debug_print(f"gateway_uuid: {hierarchy[1]} / module_uuid: {hierarchy[2]}")

    epoch_timestamp = struct.unpack('i', payload[0:4])
    timestamp = convert_epoch_to_mysql_timestamp(epoch_timestamp[0])

    read_start = 5
    n_values = payload[4]
    for i in range(n_values):
        try:
            id = payload[read_start + i*6:read_start + i*6 + 1]
            data = payload[read_start + i*6+2:read_start+(i+1)*6]
            value = struct.unpack('f', data)[0]

            # if not isinstance(value, int) or not isinstance(value, float):
            if str(value) == "nan" or math.isnan(value):
                debug_print('Invalid value received, not inserting data of sensortype {} in database !!'.format(id))
                #                print('Invalid value received, not inserting data of sensortype {} in database !!'.format(id))
                debug_print('\nInvalid value = {}'.format(value))
            #               print('\nInvalid value = {}'.format(value))
            else:
                debug_print("id: {} - value {}".format(id, value))
                # debug_print("id: {} - value {}".format(id, value))

                #              print("[MQTT message] " + str(gateway_uuid) + " | " + str(module_uuid) + " | " + str(id) + " | " + str(value) + " |\t " + str(epoch_timestamp) + " | " + str(get_current_timestamp()))
                debug_print(
                    "[MQTT message] " + str(gateway_uuid) + " | " + str(module_uuid) + " | " + str(id) + " | " + str(
                        value) + " |\t " + str(epoch_timestamp) + " | " + str(get_current_timestamp()))

                result = mysql_manager.process_sensor_module_message(module_uuid, gateway_uuid, id, value, timestamp)

                debug_print("[MySQL] insert status: " + ("OK " if result[0] else "WARNING dup id "))
        #             print("[MySQL] insert status: " + ("OK " if result[0] else "WARNING dup id "))
        except Exception as e:
            debug_print("Exception during unpacking measurement: {} ".format(e))
    #        print("Exception during unpacking measurement: {} ".format(e))

if __name__ == "__main__":
    print("Starting MQTT message handler.")
    client = mqtt.Client()

    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(config.mqqt_settings['broker'], config.mqqt_settings['port'], 60)

    # Blocking call that processes network traffic, dispatches callbacks and
    # handles reconnecting.
    # Other loop*() functions are available that give a threaded interface and a
    # manual interface.
    print("[MQTT] Client background thread started")
    while 1:
        client.loop()
