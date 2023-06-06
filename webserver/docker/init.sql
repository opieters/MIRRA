-- --------------------------------------------------------
-- Host:                         127.0.0.1
-- Server version:               5.7.25 - MySQL Community Server (GPL)
-- Server OS:                    Linux
-- HeidiSQL Version:             12.5.0.6677
-- --------------------------------------------------------

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET NAMES utf8 */;
/*!50503 SET NAMES utf8mb4 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;


-- Dumping database structure for mms
DROP DATABASE IF EXISTS `mms`;
CREATE DATABASE IF NOT EXISTS `mms` /*!40100 DEFAULT CHARACTER SET latin1 */;
USE `mms`;

-- Dumping structure for table mms.forests
DROP TABLE IF EXISTS `forests`;
CREATE TABLE IF NOT EXISTS `forests` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(128) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `name_UNIQUE` (`name`),
  UNIQUE KEY `id_UNIQUE` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4;

-- Dumping structure for table mms.gateways
DROP TABLE IF EXISTS `gateways`;
CREATE TABLE IF NOT EXISTS `gateways` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(128) DEFAULT NULL,
  `friendly_name` varchar(45) DEFAULT NULL,
  `location_id` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `name_UNIQUE` (`name`),
  KEY `gateway_location` (`location_id`),
  CONSTRAINT `gateway_location` FOREIGN KEY (`location_id`) REFERENCES `locations` (`id`) ON DELETE NO ACTION ON UPDATE NO ACTION
) ENGINE=InnoDB AUTO_INCREMENT=10 DEFAULT CHARSET=utf8mb4;

-- Dumping structure for table mms.locations
DROP TABLE IF EXISTS `locations`;
CREATE TABLE IF NOT EXISTS `locations` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `friendly_name` varchar(64) NOT NULL DEFAULT '0',
  `lat` decimal(32,8) NOT NULL,
  `lng` decimal(32,8) NOT NULL,
  `forest_id` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `location_forest_id` (`forest_id`),
  CONSTRAINT `location_forest_id` FOREIGN KEY (`forest_id`) REFERENCES `forests` (`id`) ON DELETE NO ACTION ON UPDATE NO ACTION
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4;

-- Dumping structure for table mms.sensor_classes
DROP TABLE IF EXISTS `sensor_classes`;
CREATE TABLE IF NOT EXISTS `sensor_classes` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `class` varchar(45) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `id_UNIQUE` (`id`),
  UNIQUE KEY `class_UNIQUE` (`class`)
) ENGINE=InnoDB AUTO_INCREMENT=23 DEFAULT CHARSET=utf8mb4;

-- Dumping data for table mms.sensor_classes: ~5 rows (approximately)
INSERT INTO `sensor_classes` (`id`, `class`) VALUES
	(11, 'AIR_HUMIDITY'),
	(10, 'AIR_TEMPERATURE'),
	(22, 'PAR'),
	(3, 'SOIL_HUMIDITY'),
	(4, 'SOIL_TEMPERATURE');

-- Dumping structure for table mms.sensor_measurements
DROP TABLE IF EXISTS `sensor_measurements`;
CREATE TABLE IF NOT EXISTS `sensor_measurements` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `timestamp` varchar(50) NOT NULL DEFAULT '0',
  `sensor_module_id` int(11) NOT NULL,
  `sensor_type_id` int(11) NOT NULL,
  `gateway_id` int(11) NOT NULL,
  `value` decimal(32,4) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `id_UNIQUE` (`id`),
  UNIQUE KEY `unique_index` (`timestamp`,`sensor_module_id`,`sensor_type_id`,`gateway_id`,`value`),
  KEY `sensor_type_fk_idx` (`sensor_type_id`),
  CONSTRAINT `measurement_sensor_type_fk` FOREIGN KEY (`sensor_type_id`) REFERENCES `sensor_types` (`id`) ON DELETE NO ACTION ON UPDATE NO ACTION
) ENGINE=InnoDB AUTO_INCREMENT=119 DEFAULT CHARSET=utf8mb4;

-- Dumping structure for table mms.sensor_modules
DROP TABLE IF EXISTS `sensor_modules`;
CREATE TABLE IF NOT EXISTS `sensor_modules` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `name` varchar(45) DEFAULT NULL,
  `location_id` int(11) DEFAULT NULL,
  `friendly_name` varchar(45) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `id_UNIQUE` (`id`),
  UNIQUE KEY `name_UNIQUE` (`name`),
  UNIQUE KEY `friendly_name_UNIQUE` (`friendly_name`),
  KEY `sensor_module_fk` (`location_id`),
  CONSTRAINT `sensor_module_fk` FOREIGN KEY (`location_id`) REFERENCES `locations` (`id`) ON DELETE NO ACTION ON UPDATE NO ACTION
) ENGINE=InnoDB AUTO_INCREMENT=6 DEFAULT CHARSET=utf8mb4;

-- Dumping structure for table mms.sensor_types
DROP TABLE IF EXISTS `sensor_types`;
CREATE TABLE IF NOT EXISTS `sensor_types` (
  `id` int(11) NOT NULL,
  `name` varchar(128) NOT NULL,
  `class` int(11) NOT NULL,
  `unit` varchar(45) NOT NULL,
  `accuracy` decimal(64,1) NOT NULL,
  `description` varchar(1024) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `id_UNIQUE` (`id`),
  KEY `fk_sensor_types_sensor_classes1_idx` (`class`),
  CONSTRAINT `fk_sensor_types_sensor_classes1` FOREIGN KEY (`class`) REFERENCES `sensor_classes` (`id`) ON DELETE NO ACTION ON UPDATE NO ACTION
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Dumping data for table mms.sensor_types: ~2 rows (approximately)
INSERT INTO `sensor_types` (`id`, `name`, `class`, `unit`, `accuracy`, `description`) VALUES
	(3, 'Soil Moisture', 3, '%RH', 2.0, 'Soil Moisture'),
	(4, 'Soil Temp', 4, 'C', 0.5, 'Soil Temperature.'),
	(12, 'SHT35', 12, 'C', 0.1, 'Temperature sensor'),
	(13, 'SHT35', 13, 'RH', 0.5, 'Relative humidity sensor'),
	(22, 'APDS', 22, 'A.U.', 1.0, 'Light sensor.');

/*!40103 SET TIME_ZONE=IFNULL(@OLD_TIME_ZONE, 'system') */;
/*!40101 SET SQL_MODE=IFNULL(@OLD_SQL_MODE, '') */;
/*!40014 SET FOREIGN_KEY_CHECKS=IFNULL(@OLD_FOREIGN_KEY_CHECKS, 1) */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40111 SET SQL_NOTES=IFNULL(@OLD_SQL_NOTES, 1) */;
