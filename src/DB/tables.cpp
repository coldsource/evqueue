/*
 * This file is part of evQueue
 * 
 * evQueue is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * evQueue is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with evQueue. If not, see <http://www.gnu.org/licenses/>.
 * 
 * Author: Thibault Kummer <bob@coldsource.net>
 */

#include <string>
#include <map>

std::map<std::string,std::string> evqueue_tables = {
{"t_log",
"CREATE TABLE `t_log` ( \
  `log_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `node_name` varchar(32) CHARACTER SET ascii NOT NULL DEFAULT '', \
  `log_level` int(11) NOT NULL, \
  `log_message` text COLLATE utf8_unicode_ci NOT NULL, \
  `log_timestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, \
  PRIMARY KEY (`log_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v3.1'; \
"},
{"t_notification",
"CREATE TABLE `t_notification` ( \
  `notification_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `notification_type_id` int(10) unsigned NOT NULL, \
  `notification_name` varchar(64) COLLATE utf8_unicode_ci NOT NULL, \
  `notification_subscribe_all` int(10) unsigned NOT NULL DEFAULT 0, \
  `notification_parameters` longtext COLLATE utf8_unicode_ci NOT NULL, \
  PRIMARY KEY (`notification_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v3.1'; \
"},
{"t_notification_type",
"CREATE TABLE `t_notification_type` ( \
  `notification_type_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `notification_type_name` varchar(32) COLLATE utf8_unicode_ci NOT NULL, \
  `notification_type_description` text COLLATE utf8_unicode_ci NOT NULL, \
  `notification_type_manifest` longtext COLLATE utf8_unicode_ci NOT NULL, \
  `notification_type_binary_content` longblob, \
  `notification_type_conf_content` longblob, \
  PRIMARY KEY (`notification_type_id`), \
  UNIQUE KEY `notification_type_name` (`notification_type_name`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v3.1'; \
"},
{"t_queue",
"CREATE TABLE `t_queue` ( \
  `queue_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `queue_name` varchar(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, \
  `queue_concurrency` int(10) unsigned NOT NULL, \
  `queue_scheduler` VARCHAR(32) COLLATE 'ascii_general_ci' NOT NULL DEFAULT 'default', \
  `queue_dynamic` TINYINT NOT NULL DEFAULT 0, \
  PRIMARY KEY (`queue_id`), \
  UNIQUE KEY `queue_name` (`queue_name`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='v3.1'; \
"},
{"t_schedule",
"CREATE TABLE `t_schedule` ( \
  `schedule_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `schedule_name` varchar(64) COLLATE utf8_unicode_ci NOT NULL, \
  `schedule_xml` text COLLATE utf8_unicode_ci NOT NULL, \
  PRIMARY KEY (`schedule_id`), \
  UNIQUE KEY `schedule_name` (`schedule_name`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v3.1'; \
"},
{"t_user",
"CREATE TABLE `t_user` ( \
  `user_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `user_login` varchar(32) COLLATE utf8_unicode_ci NOT NULL, \
  `user_password` varchar(40) CHARACTER SET ascii NOT NULL, \
  `user_password_salt` varchar(40) CHARACTER SET ascii DEFAULT NULL, \
  `user_password_iterations` int(11) NOT NULL DEFAULT 0, \
  `user_profile` enum('ADMIN','USER') COLLATE utf8_unicode_ci NOT NULL DEFAULT 'USER', \
  `user_preferences` text CHARACTER SET ascii NOT NULL, \
  PRIMARY KEY (`user_id`), \
  UNIQUE KEY `user_login` (`user_login`) \
) ENGINE=InnoDB AUTO_INCREMENT=4 DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v3.1'; \
"},
{"t_user_right",
"CREATE TABLE `t_user_right` ( \
  `user_id` int(10) unsigned NOT NULL, \
  `workflow_id` int(10) NOT NULL, \
  `user_right_edit` tinyint(1) NOT NULL DEFAULT 0, \
  `user_right_read` tinyint(1) NOT NULL DEFAULT 0, \
  `user_right_exec` tinyint(1) NOT NULL DEFAULT 0, \
  `user_right_kill` tinyint(4) NOT NULL DEFAULT 0, \
  PRIMARY KEY (`user_id`,`workflow_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v3.1'; \
"},
{"t_workflow",
"CREATE TABLE `t_workflow` ( \
  `workflow_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `workflow_name` varchar(64) COLLATE utf8_unicode_ci NOT NULL, \
  `workflow_xml` longtext COLLATE utf8_unicode_ci NOT NULL, \
  `workflow_group` varchar(64) COLLATE utf8_unicode_ci NOT NULL, \
  `workflow_comment` text COLLATE utf8_unicode_ci NOT NULL, \
  `workflow_lastcommit` varchar(40) COLLATE 'ascii_general_ci' NULL DEFAULT NULL, \
  UNIQUE KEY `workflow_id` (`workflow_id`), \
  UNIQUE KEY `workflow_name` (`workflow_name`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v3.1'; \
"},
{"t_workflow_instance",
"CREATE TABLE `t_workflow_instance` ( \
  `workflow_instance_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `node_name` varchar(32) CHARACTER SET ascii NOT NULL DEFAULT '', \
  `workflow_id` int(10) NOT NULL, \
  `workflow_schedule_id` int(10) unsigned DEFAULT NULL, \
  `workflow_instance_host` varchar(64) COLLATE utf8_unicode_ci DEFAULT NULL, \
  `workflow_instance_start` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP, \
  `workflow_instance_end` timestamp NULL, \
  `workflow_instance_status` enum('EXECUTING','TERMINATED','ABORTED') COLLATE utf8_unicode_ci NOT NULL, \
  `workflow_instance_errors` int(10) unsigned NULL, \
  `workflow_instance_comment` varchar(255) COLLATE utf8_unicode_ci DEFAULT NULL, \
  `workflow_instance_savepoint` longtext COLLATE utf8_unicode_ci NULL DEFAULT NULL, \
  UNIQUE KEY `workflow_instance_id` (`workflow_instance_id`), \
  KEY `workflow_instance_status` (`workflow_instance_status`), \
  KEY `workflow_instance_date_end` (`workflow_instance_end`), \
  KEY `t_workflow_instance_errors` (`workflow_instance_errors`), \
  KEY `workflow_instance_date_start` (`workflow_instance_start`), \
  KEY `workflow_schedule_id` (`workflow_schedule_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v3.1'; \
"},
{"t_workflow_instance_parameters",
"CREATE TABLE `t_workflow_instance_parameters` ( \
  `workflow_instance_id` int(10) unsigned NOT NULL, \
  `workflow_instance_parameter` varchar(64) COLLATE utf8_unicode_ci NOT NULL, \
  `workflow_instance_parameter_value` text COLLATE utf8_unicode_ci NOT NULL, \
  KEY `param_and_value` (`workflow_instance_parameter`,`workflow_instance_parameter_value`(255)) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v3.1'; \
"},
{"t_workflow_notification",
"CREATE TABLE `t_workflow_notification` ( \
  `workflow_id` int(10) unsigned NOT NULL, \
  `notification_id` int(10) unsigned NOT NULL, \
  PRIMARY KEY (`workflow_id`,`notification_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v3.1'; \
"},
{"t_workflow_schedule",
"CREATE TABLE `t_workflow_schedule` ( \
  `workflow_schedule_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `node_name` varchar(32) CHARACTER SET ascii NOT NULL DEFAULT '', \
  `workflow_id` int(10) unsigned NOT NULL, \
  `workflow_schedule` varchar(255) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, \
  `workflow_schedule_onfailure` enum('CONTINUE','SUSPEND') COLLATE utf8_unicode_ci NOT NULL, \
  `workflow_schedule_user` varchar(32) COLLATE utf8_unicode_ci DEFAULT NULL, \
  `workflow_schedule_host` varchar(64) COLLATE utf8_unicode_ci DEFAULT NULL, \
  `workflow_schedule_active` tinyint(4) NOT NULL, \
  `workflow_schedule_comment` text COLLATE utf8_unicode_ci NOT NULL, \
  PRIMARY KEY (`workflow_schedule_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v3.1'; \
"},
{"t_workflow_schedule_parameters",
"CREATE TABLE `t_workflow_schedule_parameters` ( \
  `workflow_schedule_id` int(10) unsigned NOT NULL, \
  `workflow_schedule_parameter` varchar(64) COLLATE utf8_unicode_ci NOT NULL, \
  `workflow_schedule_parameter_value` text COLLATE utf8_unicode_ci NOT NULL, \
  KEY `workflow_schedule_id` (`workflow_schedule_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v3.1'; \
"},
{"t_datastore",
"CREATE TABLE `t_datastore` ( \
  `datastore_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `workflow_instance_id` int(10) unsigned NOT NULL, \
  `datastore_value` longtext COLLATE utf8_unicode_ci NOT NULL, \
  PRIMARY KEY (`datastore_id`), \
  KEY `workflow_instance_id` (`workflow_instance_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v3.1'; \
"},
{"t_uniqueaction",
"CREATE TABLE `t_uniqueaction` ( \
  `uniqueaction_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `node_name` varchar(32) CHARACTER SET ascii NOT NULL DEFAULT '', \
  `uniqueaction_name` varchar(64) CHARACTER SET ascii NOT NULL DEFAULT '', \
  `uniqueaction_time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP, \
  PRIMARY KEY (`uniqueaction_id`), \
  KEY `uniqueaction_name` (`uniqueaction_name`,`uniqueaction_time`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v3.1'; \
"},
{"t_tag",
"CREATE TABLE `t_tag` ( \
  `tag_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `tag_label` varchar(64) CHARACTER SET utf8 NOT NULL, \
  PRIMARY KEY (`tag_id`), \
  UNIQUE KEY `tag_label` (`tag_label`) \
) ENGINE=InnoDB AUTO_INCREMENT=6 DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v3.1'; \
"},
{"t_workflow_instance_tag",
"CREATE TABLE `t_workflow_instance_tag` ( \
  `tag_id` int(10) unsigned NOT NULL, \
  `workflow_instance_id` int(10) unsigned NOT NULL, \
  PRIMARY KEY (`tag_id`,`workflow_instance_id`), \
  KEY `workflow_instance_id` (`workflow_instance_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v3.1'; \
"},
{"t_log_api",
"CREATE TABLE `t_log_api` ( \
  `log_api_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `node_name` varchar(32) CHARACTER SET ascii NOT NULL, \
  `user_id` int(10) unsigned NOT NULL, \
  `log_api_object_id` int(10) unsigned NOT NULL, \
  `log_api_object_type` enum('Workflow','WorkflowSchedule','RetrySchedule','User','Tag','Queue') CHARACTER SET ascii COLLATE ascii_bin NOT NULL, \
  `log_api_group` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, \
  `log_api_action` varchar(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, \
  `log_api_timestamp` timestamp NOT NULL DEFAULT current_timestamp(), \
  PRIMARY KEY (`log_api_id`) \
) ENGINE=InnoDB AUTO_INCREMENT=948 DEFAULT CHARSET=utf8mb4 COMMENT='v3.1'; \
"},
{"t_log_notifications",
"CREATE TABLE `t_log_notifications` ( \
  `log_notifications_id` int(10) unsigned NOT NULL AUTO_INCREMENT, \
  `node_name` varchar(32) CHARACTER SET ascii NOT NULL DEFAULT '', \
  `log_notifications_pid` INT UNSIGNED NOT NULL, \
  `log_notifications_message` text COLLATE utf8_unicode_ci NOT NULL, \
  `log_notifications_timestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, \
  PRIMARY KEY (`log_notifications_id`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v3.1'; \
"},
{"t_workflow_instance_filters",
"CREATE TABLE `t_workflow_instance_filters` ( \
  `workflow_instance_id` int(10) unsigned NOT NULL, \
  `workflow_instance_filter` varchar(64) CHARACTER SET utf8 NOT NULL, \
  `workflow_instance_filter_value` varchar(255) CHARACTER SET utf8 NOT NULL, \
  KEY `workflow_instance_filter` (`workflow_instance_filter`,`workflow_instance_filter_value`) \
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci COMMENT='v3.1'; \
"},
};
