#from influxdb import InfluxDBClient

"""
Deprecated as InfluxDB is not being used in this version (yet).
Kept as a possible future reference.
"""
class influxdb_manager:
    def __init__(self, host, port,db_name):
        self.host = host
        self.port = port
        self.db_name = db_name
        self.client =InfluxDBClient(host=self.host,port=self.port)
        self.client.switch_database(db_name)
        #print self.client.get_list_database()
        print("Database successfully connected")

    def insert_json(self,json):
        #return true if the write was successful
        return self.client.write_points(json)

    def get_all_forests(self):
        result = self.client.query("select distinct(forest_id) from sensorEvents group by forest_id;")

        print(result)