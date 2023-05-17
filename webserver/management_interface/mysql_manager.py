# -*- coding: utf-8 -*-
"""
    *****************
    mysql_manager.py
    *****************

    This file contains all the functions that are used throughout the application to interact with the database.
    These functions provide an abstraction from the specific and sometimes complex queries that are used to perform certain actions on the platform.
    Therefore, the database must only be manipulated by using these function and not by using this class to create your own queries.
"""

import pymysql
from utils import get_current_timestamp, debug_print


class mysql_manager:
    def __init__(self, host, port, db_name, username, password):
        """
        Constructor function for the database class.

        :param str host: The hostname (IP or DNS name) of the database server
        :param int port: The port the MySQL server runs on (default is 3306)
        :param str db_name: The name of the database
        :param str username: The name of the user to log in as, need full write and read access to the database.
        :param str password: The password of the user
        """
        self.host = host
        self.port = port
        self.db_name = db_name
        self.username = username
        self.password = password

    def __create_connection(self):
        """
        This private function needs to be called for every query made to the database.
        This is common practise, as with a global connection in this class,
        the connection will time out eventually and the application will need to be restarted.

        :return: PyMyQSL connection object with a pymsql.cursors.DictCursor cursor
        """
        return pymysql.connect(host=self.host,
                               user=self.username,
                               password=self.password,
                               db=self.db_name,
                               cursorclass=pymysql.cursors.DictCursor)

    def __get_module_id(self, module_uuid):
        """
        Private function that gets the database row table id for the module that is needed
        for filtering data in other methods, given the module UUID (usually the MAC address).

        :param module_uuid: The UUID of the module (MAC address) to search the ID for.
        :return: The row ID of the module when found, 0 when not found
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("SELECT id FROM sensor_modules where name = %s",
                           (module_uuid,))
            modules = cursor.fetchone()
        finally:
            connection.close()

        if modules is not None:
            return modules['id']
        else:
            return 0

    def __get_gateway_id(self, gateway_uuid):
        """
        Private function that gets the database row table id for the gateway that
        is needed for further filtering in this class, given the gateway's UUI
        (MAC address)

        :param gateway_uuid: The UUID of the module (MAC address) to search the ID for.
        :return: The row ID of the gateway when found, 0 when not found.
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("SELECT id FROM gateways where name = %s",
                           (gateway_uuid,))
            gateway = cursor.fetchone()
        finally:
            connection.close()

        if gateway is not None:
            return gateway['id']
        else:
            return 0

    def add_new_sensor_module(self, name, location_id, friendly_name):
        """
        This function adds a new sensor module to the database.

        :param str name: The name identifier of the sensor module, concrete the MAC address of the module.
        :param location_id: The ID of the location of the sensor module
        :param str friendly_name: A friendly, easy to humanly recognise name for the sensor module.
        :return: The row ID of the last inserted sensor module, False when an error occurred.
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("""INSERT INTO sensor_modules VALUES(NULL,%s,%s,%s)""",
                           (name, location_id, friendly_name))
            connection.commit()
            return cursor.lastrowid
        except:
            return False
        finally:
            connection.close()

    def __insert_sensor_measurement(self, timestamp, sensor_module_id, sensor_type_id, gateway_id, value):
        """
        Private function that insert a sensor reading into the database.

        :param timestamp: The timestamp at which the measurement occurred,
            cannot be None here, this must be checked by another function
            calling this one, more precisely, the <i>process_sensor_module()</i> function.
        :param sensor_module_id: The sensor module's ID in the database.
        :param sensor_type_id: The ID of the sensor's type in the database.
        :param gateway_id: The gateway's ID in the database.
        :param value: The value of the measurement.
        :return: True on successful insert, False on error.
        """
        connection = self.__create_connection()
        try:
            if not self.check_datapoint_exists(timestamp, sensor_module_id, sensor_type_id, gateway_id, value):
                cursor = connection.cursor()
                cursor.execute("""INSERT IGNORE INTO sensor_measurements VALUES(NULL,%s,%s,%s,%s,%s)""",
                               (timestamp, sensor_module_id, sensor_type_id, gateway_id, value))
                connection.commit()
                return True
        except Warning as e:
            print(e)
            return False
        finally:
            connection.close()

    def process_sensor_module_message(self, module_uuid, gateway_uuid, sensor_type, value, timestamp):
        """
        The main message processing function of this class that should be called
        by the front end receiving the raw messages.
        It bundles the ID lookup functions into a complete message parsing function

        :param module_uuid: The sensor module's UUID (MAC address).
        :param gateway_uuid: The gateway's UUID (MAC address).
        :param sensor_type: The sensor reading type ID.
        :param value: The actual value of the measurement.
        :param timestamp: The timestamp received in the message.
            When None is given, the function will use the timestamp of
            when the function was called as timestamp for the measurement.
        :return: (True,"Success, <info_message>") on Success, (False, <error_message>) on error.
        :rtype: tuple
        """
        inserted_time = timestamp

        # has been disabled due to a bug regarding the switch to delayed sensor readings submission
        # if timestamp is (not None or ""):
        #     inserted_time = timestamp
        # else:
        #     inserted_time = get_current_timestamp()

        # check if gateway exists
        gateway_id = self.__get_gateway_id(gateway_uuid)
        debug_print(gateway_id)

        if int(gateway_id) == 0:
            gateway_id = self.add_gateway(gateway_uuid)  # the gateway must be added first
            debug_print('new gateway detected')
        debug_print(gateway_id)

        sensor_module_id = self.__get_module_id(module_uuid)
        if sensor_module_id == 0:
            # add new unclaimed sensor
            inserted_row_id = self.add_new_sensor_module(module_uuid, None, None)
            if inserted_row_id is not False:
                # insert sensor reading
                if (self.__insert_sensor_measurement(inserted_time, inserted_row_id, sensor_type,
                                                     gateway_id, value)):
                    print(True, "Success, new unclaimed module")
                    return True, "Success, new unclaimed module"
                else:
                    print(False, "Error when persisting the measurement")
                    return False, "Error when persisting the measurement"
            else:
                print(False, "Error adding new sensor module")
                return False, "Error adding new sensor module"
        else:  # inserts are not generalised so we can pass detailed status messages
            # insert sensor reading
            if self.__insert_sensor_measurement(inserted_time, sensor_module_id, sensor_type, gateway_id, value):
                print(True, "Success")
                return True, "Success"
            else:
                return False, "Error when persisting the measurement"

    def get_all_forests(self):
        """
        This function simply gets all the forests and returns them.

        :return: The forests with all their properties.
        :rtype: list
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("SELECT * FROM forests;")
            forests_results = cursor.fetchall()
            forests = []
            for forest in forests_results:
                forests.append(forest)

            return forests

        finally:
            connection.close()

    def get_all_gateways(self):
        """
        This function simply gets all the gateways and returns them.

        :return: The gateways with all their properties.
        :rtype: list
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("SELECT * FROM gateways;")
            gateways_results = cursor.fetchall()
            gateways = []
            for gateway in gateways_results:
                gateways.append(gateway)

            return gateways

        finally:
            connection.close()


    def get_latest_measurements_from_sensor(self,sensor_module_id):
        """
        This function will get the latest sensor measurements from the passed sensor.
        :param sensor_module_id: the modules ID to get the information of.
        :return: dict
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("""select * from sensor_measurements 
                              join sensor_types on sensor_measurements.sensor_type_id = sensor_types.id
                              where sensor_module_id = %s
                              and sensor_type_id in (
                                   SELECT distinct sensor_type_id FROM sensor_measurements where sensor_module_id = %s
                              )
                              and (timestamp > DATE_SUB(now(), INTERVAL 7 DAY))
                              order by timestamp desc
                              limit 8;
                              """,
                           (sensor_module_id,sensor_module_id,))

            results = cursor.fetchall()
        finally:
            connection.close()

            return results

    def get_all_locations(self):
        """
        This function gets all forests and locations and returns them.

        :return: The forests and locations with all their properties.
        :rtype: dict
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("""SELECT forests.id as forest_id, forests.name as forest_name, locations.friendly_name as location_name, locations.id as location_id, lat, lng
                            FROM locations INNER JOIN forests
                            ON forests.id = locations.forest_id""")

            return cursor.fetchall()

        finally:
            connection.close()


    def get_all_sensors(self):
        """
        This function simply gets all the sensors and returns them.

        :return: The sensors with all their properties.
        :rtype: dict
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("""SELECT * FROM sensor_modules
                              join locations on locations.id = sensor_modules.location_id;""")

            return cursor.fetchall()

        finally:
            connection.close()

    def get_all_sensors_from_location(self, loc_id):
        """
        This function gets all the sensors from a certain location with location_id loc_id and returns them.
        :param loc_id: the location's id to get the sensors from
	:return: The sensors with all their properties.
        :rtype: dict
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("""SELECT * FROM sensor_modules
                              join locations on locations.id = sensor_modules.location_id
					WHERE locations.id = %s;""",
				   (loc_id))

            return cursor.fetchall()

        finally:
            connection.close()

    def get_forest_module_gateway_count(self):
        """
        This function returns the count of the amount of modules,
        forests and gateways that are in the system.
        This is used on the homepage to render the information cards.

        :return: dictionary with keys 'forest_count', 'module_count',
            'gateway_count' with the count as values.
        :rtype: dict
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("SELECT count(distinct(id)) as forests FROM forests")
            forest_count = cursor.fetchone()

            cursor = connection.cursor()
            cursor.execute("SELECT count(distinct(id)) as modules FROM sensor_modules")
            module_count = cursor.fetchone()

            cursor = connection.cursor()
            cursor.execute("SELECT count(distinct(id)) as gateways FROM gateways")
            gateway_count = cursor.fetchone()

        finally:
            connection.close()

        return {
            'forest_count': forest_count,
            'module_count': module_count,
            'gateway_count': gateway_count
        }

    def get_database_statistics(self):
        """
        This function gets some statistics from the database such as
        number of added points over time.

        :return: dictionary with keys  'total_datapoints', 'daily_added_datapoints',
            'weekly_added_datapoints', 'monthly_added_datapoints' and 'total_days'.
        :rtype: dict
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("SELECT count(*) as sensor_count FROM sensor_measurements")
            total_datapoints = cursor.fetchone()

            cursor = connection.cursor()
            cursor.execute(
                "SELECT count(*) daily_measurements FROM sensor_measurements "
                "WHERE timestamp > DATE_SUB(NOW(), INTERVAL 24 HOUR) AND timestamp <= NOW()")
            daily_datapoints = cursor.fetchone()

            cursor = connection.cursor()
            cursor.execute(
                "SELECT count(*) weekly_measurements FROM sensor_measurements "
                "WHERE timestamp > DATE_SUB(NOW(), INTERVAL 1 WEEK) AND timestamp <= NOW()")
            weekly_datapoints = cursor.fetchone()

            cursor = connection.cursor()
            cursor.execute(
                "SELECT count(*) monthly_measurements FROM sensor_measurements "
                "WHERE timestamp > DATE_SUB(NOW(), INTERVAL 1 WEEK) AND timestamp <= NOW()")
            monthly_datapoints = cursor.fetchone()

            cursor = connection.cursor()
            cursor.execute("""SELECT DATEDIFF(
                (SELECT max(timestamp) FROM mms.sensor_measurements),
                (SELECT min(timestamp) FROM mms.sensor_measurements)
                                            ) as time_range;""")
            total_days = cursor.fetchone()

        finally:
            connection.close()
            return {
                'total_datapoints': total_datapoints,
                'daily_added_datapoints': daily_datapoints,
                'weekly_added_datapoints': weekly_datapoints,
                'monthly_added_datapoints': monthly_datapoints,
                'total_days': total_days
            }

    def get_sensor_classes(self):
        """
        This function simply gets all the sensor classes and returns them as a dictionary.

        :return: a dictionary of all the sensor_classes.
        :rtype: dict
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("SELECT * FROM sensor_classes;")
            sensor_classes = cursor.fetchall()

            return sensor_classes

        finally:
            connection.close()

    def get_data_interval(self):
        """
        This function will determine and return the time range in which the all
        the measurements are. So it will get the timestamps at which the earliest
        measurement and the latest measurement were recorded.

        :return: dictionary with keys 'min_date' and 'max_date'
            with a MySQL compatible timestamp format
        :rtype: dict
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("SELECT max(timestamp) as max_date FROM sensor_measurements;")
            max_date = cursor.fetchone()

            cursor = connection.cursor()
            cursor.execute("SELECT min(timestamp) as min_date FROM sensor_measurements;")
            min_date = cursor.fetchone()
        finally:
            connection.close()
            return {
                'min_date': min_date['min_date'],
                'max_date': max_date['max_date']
            }

    def add_new_location(self, forest_id, location_name, lat, lng):
        """
        This function takes a geographical location defined by a latitude and
        longitude and inserts a new location for it in the database.

        :param lat: The latitude of the point.
        :param lng: The longitude of the point.
        :return: The inserted row id on success, else False.
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("INSERT INTO locations VALUES(NULL,%s,%s,%s,%s)", (location_name, lat, lng, forest_id))
            connection.commit()
            return cursor.lastrowid
        except:
            return False
        finally:
            connection.close()
    
    def remove_location(self, location_id):
        """
        This function removes a forest from the database.

        :param forest_id: The id of the forest to be removed.
        :return: True on success, False when not
        :rtype: bool
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("DELETE FROM locations WHERE id = %s", (location_id,))
            connection.commit()
            return True
        except:
            return False
        finally:
            connection.close()

    def __get_location_id(self, lat, lng):
        """
        This function gets the database table row id of the location entry
        that matches the given coordinates.

        :param lat: The latitude of the location.
        :param lng: The longitude of the location.

        :return: The row id of the location if found, 0 when not found in the database.
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("SELECT id FROM locations where lat = %s and lng = %s",
                           (lat, lng))
            location = cursor.fetchone()

        finally:
            connection.close()

        if location is not None:
            return location['id']
        else:
            return 0

    def assign_sensor_module(self, sensor_module_id, location_id, friendly_name):
        """
        This function assigns a given sensor_module to a forest.

        :param sensor_module_id: The ID of the sensor module.
        :param location_id: The ID of the location.
        :param str friendly_name: The friendly name that should be given to the sensor_module.
        :param lat: The latitude of the sensor module's location.
        :param lng: The longitude of the sensor module's location.
        :return:
        """
        connection = self.__create_connection()

        try:
            cursor = connection.cursor()
            cursor.execute("""UPDATE sensor_modules SET 
                location_id = %s,
                friendly_name = %s
                WHERE id = %s;""", (location_id, friendly_name, sensor_module_id))
            connection.commit()
            return True
        except:
            return False
        finally:
            connection.close()

    def get_all_sensor_readings_from_location(self, location_ids, min_date, max_date, sensor_classes):
        """
        This function gathers all the measurements of chosen classes
        of all the sensors in the given location(s).

        :param location_ids: List: The list of location IDs to search in.
        :param min_date: The earliest date of which measurements should be included.
        :param max_date: The latest date of which measurements should be included.
        :param sensor_classes: List: The list of types of sensors (expressed in sensor classes IDs) that need to be included.
        :return: (True, <List_of_results>) on success, (False, <error_message>) on error.
        :rtype: tuple
        """
        connection = self.__create_connection()

        location_ids_in_list = "(" + ",".join("'" + connection.escape_string(str(x)) + "'" for x in location_ids) + ")"
        classes_in_list = "(" + ",".join("'" + connection.escape_string(str(x)) + "'" for x in sensor_classes) + ")"

        try:
            cursor = connection.cursor()

            cursor.execute("""SELECT measurements.timestamp as timestamp,
	                             modules.friendly_name as sensor_module_name,
				     modules.name as sensor_module_id,
                                     classes.class as sensor_reading_class,
                                     measurements.value as value
                              FROM sensor_measurements as measurements
                              JOIN sensor_modules as modules on measurements.sensor_module_id = modules.id
                              JOIN sensor_types as stypes on measurements.sensor_type_id = stypes.id
                              JOIN sensor_classes as classes on stypes.class = classes.id

                              WHERE classes.class in """ + classes_in_list + """
                              AND   location_id in""" + location_ids_in_list)

            result = cursor.fetchall()
            return True, result
        except Exception as e:
            return False, str(e)
        finally:
            connection.close()

    def get_unclaimed_sensors(self):
        """
        This function return all the unclaimed sensors modules,
        these are the sensor_modules that are not assigned to any forest.

        :return: List of Dict objects describing each sensor module.
        :rtype: list
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("SELECT * FROM sensor_modules where location_id is NULL;")
            unclaimed_sensor_modules = cursor.fetchall()
        finally:
            connection.close()

        sensors = []
        for item in unclaimed_sensor_modules:
            sensors.append(str(item))

        return unclaimed_sensor_modules

    def get_unclaimed_gateways(self):
        """
        This function return all the unclaimed gateways, these are
        the gateways that are not assigned to any location or do
        not have a friendly name.

        :return: List of Dict objects describing each sensor module.
        :rtype: list
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("SELECT * FROM gateways where location_id is NULL or friendly_name is NULL;")
            unclaimed_gateways = cursor.fetchall()
        finally:
            connection.close()

        sensors = []
        for item in unclaimed_gateways:
            sensors.append(str(item))

        return unclaimed_gateways

    def check_datapoint_exists(self, timestamp, sensor_uuid, sensor_type_id, gateway_uuid, value):
        """
        This function checks if a datapoint is already present in the measurements table.

        :param timestamp: The timestamp at which the measurement occurred.
        :param sensor_uuid: The UUID of the sensor module.
        :param sensor_type_id: The ID of the sensor type.
        :param gateway_uuid: The UUID of the gateway.
        :param value: The value of the measurement.
        :return: True if the datapoint exists, False when not.
        :rtype: bool
        """
        connection = self.__create_connection()
        time = ""
        if timestamp is "" or None:
            time = get_current_timestamp()
        else:
            time = timestamp
        try:
            cursor = connection.cursor()
            cursor.execute(
                """SELECT count(id) as existing FROM sensor_measurements 
                where timestamp = %s AND sensor_module_id = %s AND sensor_type_id = %s 
                    AND gateway_id = %s and value = %s""",
                (time, sensor_uuid, sensor_type_id, gateway_uuid, value))
            result = cursor.fetchone()

        finally:
            connection.close()
        return True if (result['existing'] > 0) else False

    def add_forest(self, forest_name):
        """
        This function adds a forest to the database and is used in the
        front end when submitting the form.

        :param forest_name: The easy to recognise name of the forest.
        :return: True on success, False on error.
        :rtype: bool
        """

        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("""INSERT INTO forests VALUES(NULL,%s)""", (forest_name,))
            connection.commit()
            return True
        except:
            return False
        finally:
            connection.close()

    def remove_forest(self, forest_id):
        """
        This function removes a forest from the database.

        :param forest_id: The id of the forest to be removed.
        :return: True on success, False when not
        :rtype: bool
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("DELETE FROM forests WHERE id = %s", (forest_id,))
            connection.commit()
            return True
        except:
            return False
        finally:
            connection.close()

    def remove_gateway(self, gateway_id):
        """
        This function removes a gateway from the database.

        :param gateway_id: The id of the gateway to be removed.
        :return: True on success, False when not
        :rtype: bool
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("DELETE FROM gateways WHERE id = %s", (gateway_id,))
            connection.commit()
            return True
        except:
            return False
        finally:
            connection.close()

    def unassign_sensor_module(self, sensor_id):
        """
        This function unassigns a sensor module from a forest,
        meaning the sensor module's forest id will be set to NULL.

        :param sensor_id: The id of the sensor_module to be unassigned.
        :return: True on success, False on error.
        :rtype: bool
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("""UPDATE sensor_modules SET location_id = NULL WHERE id = %s;""", (sensor_id,))
            connection.commit()
            return True
        except:
            return False
        finally:
            connection.close()

    def add_gateway(self, gateway_uuid):
        """
        This function adds a gateway to the database and is used in the front end when submitting the form.

        :param gateway_uuid: The gateway's MAC address.
        :return: True on success, False on error.
        :rtype: bool
        """
        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("""INSERT INTO gateways(`id`,`name`) VALUES(NULL,%s);""", (gateway_uuid,))
            connection.commit()
            return cursor.lastrowid
        except Exception as e:
            print(e)
            return False
        finally:
            connection.close()

    def assign_gateway(self, friendly_name, location_id, gateway_id):
        """
        This function assigns a given gateway to a location and sets a name.

        :param friendly_name: The friendly name that should be given to the gateway.
        :param lat: The latitude of the sensor module's location.
        :param lng: The longitude of the sensor module's location.
        :return:
        """

        connection = self.__create_connection()
        try:
            cursor = connection.cursor()
            cursor.execute("""UPDATE gateways SET location_id = %s, friendly_name = %s WHERE id = %s;""",
                           (location_id, friendly_name, gateway_id))
            connection.commit()
            return True
        except Exception as e:
            debug_print(e)
            return False
        finally:
            connection.close()
