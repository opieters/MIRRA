# -*- coding: utf-8 -*-
"""
    **********
    config.py
    **********

    Withholds all the configurations for the application.


    This file contains the configurations for all the aspacts of the application such as port settings, debug mode settings,
    database settings, MQTT settings and basic auth credentials.
"""

general = {
    'port': 80,  # the port to run the application on. ports under 1024 will require root/admin privileges.
    'enable_HTTP_API': False
}

debug_settings = {
    'verbose': True,  # enable verbose logging
    'timestamp': True
}

db_settings = {
    'username': 'grafana',
    'password': 'grafana-d!t-is-3cht-tOp_veilig',
    'port': 3306,
    'host': 'gems_mysql',
    'database_name': 'mms'

}

basic_auth_settings = {
    'username': 'vop',
    'password': 'cw'
}

mqqt_settings = {
    'port': 1883,
    'broker': 'mqtt_broker',
    'prefix': 'fornalab/#',
    'user': 'case12',
    'password': 'mechatronica'
}
