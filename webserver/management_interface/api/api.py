# -*- coding: utf-8 -*-
"""
    *******
    api.py
    *******

    This file contains the /api endpoints logic. For this version of the software, the API is not used. Instead a MQTT version is used.
    It can nevertheless be enabled in the config file and the sensor module's must be updated accordingly.
"""

import json
import config
from flask import Blueprint, request, jsonify, session
from mysql_manager import mysql_manager
from utils import get_current_timestamp

api = Blueprint('api', 'api', url_prefix='/api')

mysql_manager = mysql_manager(config.db_settings['host'],
                              config.db_settings['port'],
                              config.db_settings['database_name'],
                              config.db_settings['username'],
                              config.db_settings['password'])


@api.route('/sensor_data/', methods=['POST'], strict_slashes=False)
def add_sensor_data():
    if request.method == 'POST':
        if config.general['enable_HTTP_API'] is True:
            request_json = request.get_json()

            module_id = request_json['module_id'] if (request_json['module_id'] is not None) else 'NULL'
            gateway_id = request_json['gateway_id'] if (request_json['gateway_id'] is not None) else 'NULL'
            sensor_type = request_json['sensor_type'] if (request_json['sensor_type'] is not None) else 'NULL'
            timestamp = request_json['payload']['timestamp'] if (
                    request_json['payload']['timestamp'] is not None) else get_current_timestamp()
            value = request_json['payload']['value'] if (request_json['payload']['value'] is not None) else 'NULL'

            print('module: ' + str(module_id) + " | gw: " + str(gateway_id) + " | sens_type: " + str(
                sensor_type) + " | " + str(timestamp) + " | " + str(value))

            insertion = mysql_manager.process_sensor_module_message(module_id, gateway_id, sensor_type, value,
                                                                    timestamp)

            if insertion[0] is True:
                return json.dumps({'Success': True}), 200, {'ContentType': 'application/json'}
            else:
                return json.dumps({
                    'Success': False,
                    'Error_message': insertion[1]
                }), 200, {'ContentType': 'application/json'}
        else:
            pass
        return json.dumps({
            'Success': False,
            'Error_message': "HTTP API not enabled on this server"
        }), 403, {'ContentType': 'application/json'}
