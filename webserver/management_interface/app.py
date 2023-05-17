# -*- coding: utf-8 -*-
"""
    *******
    app.py
    *******

    This file contains the main entry point of the application. It sets up the whole application and handles the website's URL handlers.
"""


import config
from flask import Flask, request, render_template, make_response, flash, redirect, jsonify, send_from_directory
from flask_basicauth import BasicAuth
from mysql_manager import mysql_manager
from utils import get_current_timestamp, create_csv_report, uploaded_csv_file_to_datastream, debug_print
from api.api import api

# setup the app and register blueprints
app = Flask(__name__)
app.register_blueprint(api)
# the secret key to work in a production environment
app.secret_key = '16EBF3B63B3C5EB19D056B1D0A07D38078122D987B403F478935A1CF137D820C'

# setup basic auth
app.config['BASIC_AUTH_USERNAME'] = config.basic_auth_settings['username']
app.config['BASIC_AUTH_PASSWORD'] = config.basic_auth_settings['password']
basic_auth = BasicAuth(app)

# initialise the database manager
mysql_manager = mysql_manager(config.db_settings['host'],
                              config.db_settings['port'],
                              config.db_settings['database_name'],
                              config.db_settings['username'],
                              config.db_settings['password'])


@app.route('/')
@basic_auth.required
def index():
    """
    The default index URL handler

    :return: The index view
    """
    sensor_statistics = mysql_manager.get_forest_module_gateway_count()
    unclaimed_sensors = mysql_manager.get_unclaimed_sensors()
    unclaimed_gateways = mysql_manager.get_unclaimed_gateways()
    database_statistics = mysql_manager.get_database_statistics()

    return render_template('index.html',
                           forest_count=sensor_statistics['forest_count']['forests'],
                           # the amount of forests in the database
                           sensor_count=sensor_statistics['module_count']['modules'],
                           # the amount of unique sensor modules in the database
                           gateway_count=sensor_statistics['gateway_count']['gateways'],
                           # the amount of unique gateways in the database
                           unclaimed_sensors_count=len(unclaimed_sensors),  # the amount of unclaimed sensor modules
                           unclaimed_gateways_count=len(unclaimed_gateways),
                           total_datapoints=database_statistics['total_datapoints']['sensor_count'],
                           daily_added_datapoints=database_statistics['daily_added_datapoints']['daily_measurements'],
                           weekly_added_datapoints=database_statistics['weekly_added_datapoints'][
                               'weekly_measurements'],
                           monthly_added_datapoints=database_statistics['monthly_added_datapoints'][
                               'monthly_measurements'],
                           total_days=database_statistics['total_days']['time_range'],
                           locations=mysql_manager.get_all_locations()
                           # total days measured as recorder in the database
                           )


@app.route('/reports/', methods=['GET', 'POST'], strict_slashes=False)
@basic_auth.required
def generate_report():
    """
    The reports URL handler.

    :return: The reports view on GET, the requested CSV report on POST
    """
    if request.method == 'GET':
        dates = mysql_manager.get_data_interval()
        # todo check for min date < max date
        min_date = str(dates['min_date']).split(' ')[0]
        max_date = str(dates['max_date']).split(' ')[0]

        return render_template('reports.html',
                               locations=mysql_manager.get_all_locations(),
                               sensor_types=mysql_manager.get_sensor_classes(),
                               min_date=min_date,
                               max_date=max_date)

    elif request.method == 'POST':
        locations = request.form.getlist('locations')
        start_date = request.form['start_date']
        end_date = request.form['end_date']
        sensor_types = request.form.getlist('sensor_types')
        advanced_options = request.form.getlist('advanced_options')

        successful, result = mysql_manager.get_all_sensor_readings_from_location(locations, start_date, end_date,
                                                                               sensor_types)

        if successful:
            if len(result) > 0:  # when the requested data set has no entries, propagate this back to the user.
                # generate a csv file
                csv_data = create_csv_report(result, advanced_options)

                # send it back as the response
                response = make_response(csv_data.encode('utf-8'))
                response.headers[
                    'Content-Disposition'] = 'attachment; filename=exported_data_' + get_current_timestamp().replace(
                    "-", "_") + '.csv'
                response.mimetype = 'text/csv'
                return response
            else:
                flash(
                    'The datasheet you requested with the submitted parameters returned and empty resultset! Please check the form and try again.',
                    'warning')
        else:
            flash('An error occurred generating the report, please check your submitted parameters for correctness',
                  'danger')

        dates = mysql_manager.get_data_interval()
        min_date = str(dates['min_date']).split(' ')[0]
        max_date = str(dates['max_date']).split(' ')[0]

        return render_template('reports.html',
                               forests=mysql_manager.get_all_forests(),
                               sensor_types=mysql_manager.get_sensor_classes(),
                               min_date=min_date,
                               max_date=max_date)


@app.route('/assign/', methods=['GET', 'POST'], strict_slashes=False)
@basic_auth.required
def assign():
    """
    The assign URL handler

    :return: The assign views
    """
    if request.method == 'GET':
        unclaimed_sensors = mysql_manager.get_unclaimed_sensors()

        return render_template('assign.html',
                               unclaimed_sensors=unclaimed_sensors,
                               locations=mysql_manager.get_all_locations())
    elif request.method == 'POST':

        sensor_module_id = request.form['sensor']
        location_id = request.form['location_id']
        friendly_name = request.form['sensor_module_name']

        # if not (is_valid_float(lat) and is_valid_float(lng)):
        #     flash('The submitted coordinates do not have the correct format!','danger')
        #     return redirect('/assign/')

        if (mysql_manager.assign_sensor_module(sensor_module_id, location_id, friendly_name)):
            flash('Sensor(s) were successfully added to the forest(s)', 'success')
        else:
            flash('An error occurred assigning the sensors', 'danger')

        unclaimed_sensors = mysql_manager.get_unclaimed_sensors()
        forests = mysql_manager.get_all_forests()

        return render_template('assign.html',
                               unclaimed_sensors=unclaimed_sensors,
                               locations=mysql_manager.get_all_locations())
        # return redirect does not work anymore for some reasons


@app.route('/assign_gateway/', methods=['GET', 'POST'], strict_slashes=False)
@basic_auth.required
def assign_gateway():
    """
    The assign gateways URL handler

    :return: The assign views.
    """
    if request.method == 'GET':
        return render_template('assign_gateways.html', unclaimed_gateways=mysql_manager.get_unclaimed_gateways(),
                                                       locations=mysql_manager.get_all_locations(),)
    elif request.method == 'POST':

        gateway_id = request.form['gateway']
        friendly_name = request.form['gateway_friendly_name']
        location_id = request.form['location_id']

        if (mysql_manager.assign_gateway(friendly_name, location_id, gateway_id)):
            flash('Gateway was successfully claimed', 'success')
        else:
            flash('An error occurred claiming the gateway', 'danger')

        return render_template('assign_gateways.html', unclaimed_gateways=mysql_manager.get_unclaimed_gateways(),
                                                       locations=mysql_manager.get_all_locations(),)


@app.route('/remove_gateway/', methods=['GET', 'POST'], strict_slashes=False)
@basic_auth.required
def remove_gateway():
    """
    The remove gateway URL handler.

    :return:  The remove_forest.html page
    """
    if request.method == 'GET':
        return render_template('remove_gateway.html', gateways=mysql_manager.get_all_gateways())

    elif request.method == 'POST':
        gateway_id = request.form['gateway_id']

        if mysql_manager.remove_gateway(gateway_id):
            flash('Gateway was successfully removed!', 'success')
        else:
            flash('Error while removing the gateway!', 'danger')

        return render_template('remove_gateway.html', gateways=mysql_manager.get_all_gateways())


@app.route('/add_forest/', methods=['GET', 'POST'], strict_slashes=False)
@basic_auth.required
def add_forest():
    """
    The add forest URL handler

    :return: The add_forest.html template
    """
    if request.method == 'GET':
        return render_template('add_forest.html')
    elif request.method == 'POST':
        forest_name = request.form['forest_name']

        if mysql_manager.add_forest(forest_name):
            flash('Forest was successfully added!', 'success')
        else:
            flash('Error while adding the forest!', 'danger')
        return render_template('add_forest.html')

@app.route('/add_location/', methods=['GET', 'POST'], strict_slashes=False)
@basic_auth.required
def add_location():
    """
    The add forest URL handler

    :return: The add_location.html template
    """
    if request.method == 'GET':
        return render_template('add_location.html',
                                forests=mysql_manager.get_all_forests())
    elif request.method == 'POST':
        forest_id = request.form['forest_id']
        location_name = request.form['location_friendly_name']
        lat = request.form['lat']
        lng = request.form['lng']

        if mysql_manager.add_new_location(forest_id, location_name, lat, lng):
            flash('Location was successfully added!', 'success')
        else:
            flash('Error while adding the location!', 'danger')
        return render_template('add_location.html',
                                forests=mysql_manager.get_all_forests())

@app.route('/remove_location/', methods=['GET', 'POST'], strict_slashes=False)
@basic_auth.required
def remove_location():
    """
    The remove location URL handler.

    :return:  The remove_location.html page
    """
    if request.method == 'GET':
        return render_template('remove_location.html', locations=mysql_manager.get_all_locations())

    elif request.method == 'POST':
        location_id = request.form['location_id']

        if mysql_manager.remove_location(location_id):
            flash('Location was successfully removed!', 'success')
        else:
            flash('Error while removing the location! Is it still linked to sensors or gateways?', 'danger')

        return render_template('remove_location.html', locations=mysql_manager.get_all_locations())

@app.route('/remove_forest/', methods=['GET', 'POST'], strict_slashes=False)
@basic_auth.required
def remove_forest():
    """
    The remove forest URL handler.

    :return:  The remove_forest.html page
    """
    if request.method == 'GET':
        return render_template('remove_forest.html', forests=mysql_manager.get_all_forests())

    elif request.method == 'POST':
        forest_id = request.form['forest_id']

        if mysql_manager.remove_forest(forest_id):
            flash('Forest was successfully removed!', 'success')
        else:
            flash('Error while removing the forest!', 'danger')

        return render_template('remove_forest.html', forests=mysql_manager.get_all_forests())


@app.route('/remove_sensor/', methods=['GET', 'POST'], strict_slashes=False)
@basic_auth.required
def remove_sensor():
    """
    The remove sensor module URL handler

    :return: A JSON response {'success': boolean} with the boolean indicating
        whether the sensor module was successfully removed from the forest.
    """
    if request.method == 'GET':
        return render_template('remove_sensor.html', forests=mysql_manager.get_all_forests(),
                               sensors=mysql_manager.get_all_sensors())
    elif request.method == 'POST':
        if (mysql_manager.unassign_sensor_module(request.form['sensor_id'])):
            return jsonify({'success': True})
        else:
            return jsonify({'success': False})

@app.route('/forest_overview/', methods=['GET'], strict_slashes=False)
@basic_auth.required
def forest_overview():
    """
    The remove forest URL handler.

    :return:  The remove_forest.html page
    """

    all_sensors = mysql_manager.get_all_sensors()
    for sensor_module in all_sensors:
        sensor_module['latest_measurements'] = mysql_manager.get_latest_measurements_from_sensor(sensor_module['id'])

    return render_template('forest_overview.html', forests=mysql_manager.get_all_forests(),
                                                   locations=mysql_manager.get_all_locations(),
                                                   sensors=all_sensors)

@app.route('/sensor_overview/', methods=['GET'], strict_slashes=False)
@basic_auth.required
def sensor_overview():
    """
    Get overview of all sensors.

    :return:  The sensor_overview.html page
    """

    all_sensors = mysql_manager.get_all_sensors()
    for sensor_module in all_sensors:
        sensor_module['latest_measurements'] = mysql_manager.get_latest_measurements_from_sensor(sensor_module['id'])

    return render_template('sensor_overview.html', locations=mysql_manager.get_all_locations(),
                                                   sensors=all_sensors)

if __name__ == '__main__':
    app.run('0.0.0.0', port=80, debug=False)