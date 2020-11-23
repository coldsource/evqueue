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

#include <DB/tools_db.h>
#include <DB/tables.h>
#include <DB/DB.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <Exception/Exception.h>
#include <Logger/Logger.h>
#include <DOM/DOMDocument.h>
#include <DOM/DOMXPathResult.h>

#include <pcrecpp.h>

#include <string>
#include <vector>
#include <memory>

using namespace std;

void tools_init_db(void)
{
	DB db;
	
	Configuration *config = ConfigurationEvQueue::GetInstance();
	
	map<string,string>::iterator it;
	for(it=evqueue_tables.begin();it!=evqueue_tables.end();++it)
	{
		db.QueryPrintf(
			"SELECT table_comment FROM INFORMATION_SCHEMA.TABLES WHERE table_schema=%s AND table_name=%s",
			&config->Get("mysql.database"),
			&it->first
		);
		
		if(!db.FetchRow())
		{
			Logger::Log(LOG_NOTICE,"Table %s does not exists, creating it...",it->first.c_str());
			
			db.Query(it->second.c_str());
			
			if(it->first=="t_queue")
				db.Query("INSERT INTO t_queue(queue_name,queue_concurrency,queue_scheduler) VALUES('default',1,'default');");
			else if(it->first=="t_user")
				db.Query("INSERT INTO t_user(user_login,user_password,user_profile,user_preferences) VALUES('admin',SHA1('admin'),'ADMIN','');");
		}
		else
		{
			if(string(db.GetField(0))!="v" EVQUEUE_VERSION)
			{
				if(db.GetField(0)=="v2.0" && EVQUEUE_VERSION=="2.2")
					tools_upgrade_v20_v22();
				else if(db.GetField(0)=="v2.2" && EVQUEUE_VERSION=="3.0")
					tools_upgrade_v22_v30();
				else if(db.GetField(0)=="v2.2" && EVQUEUE_VERSION=="3.1")
				{
					tools_upgrade_v22_v30();
					tools_upgrade_v30_v31();
				}
				else if(db.GetField(0)=="v3.0" && EVQUEUE_VERSION=="3.1")
					tools_upgrade_v30_v31();
				else
					throw Exception("DB Init","Wrong table version, should be " EVQUEUE_VERSION);
			}
			
			if(it->first=="t_notification")
				tools_upgrade_t_notification();
		}
	}
}

void tools_upgrade_v20_v22(void)
{
	Logger::Log(LOG_NOTICE,"Detected v2.0 tables, upgrading scheme to v2.2");
	
	DB db;
	DB db2;
	
	db.Query("SELECT workflow_id, workflow_xml FROM t_workflow");
	while(db.FetchRow())
	{
		unsigned int id = db.GetFieldInt(0);
		string xml = db.GetField(1);
		
		unique_ptr<DOMDocument> xmldoc(DOMDocument::Parse(xml));
		
		unique_ptr<DOMXPathResult> tasks(xmldoc->evaluate("//task",xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
		
		unsigned int tasks_index = 0;
		while(tasks->snapshotItem(tasks_index++))
		{
			DOMElement task = (DOMElement)tasks->getNodeValue();
			
			// Change task's @name to @path
			if(task.hasAttribute("path"))
				continue;
			
			string name = task.getAttribute("name");
			
			string path, wd="", parameters_mode="CMDLINE",output_method="TEXT",merge_stderr="0",use_agent="0",user="",host="";
			if(name[0]=='!')
				path = name.substr(1);
			else
			{
				db2.QueryPrintf("SELECT task_parameters_mode,task_output_method,task_merge_stderr,task_use_agent,task_user,task_host,task_wd,task_binary FROM t_task WHERE task_name=%s",&name);
				if(!db2.FetchRow())
					path = name;
				else
				{
					parameters_mode = db2.GetField(0);
					output_method = db2.GetField(1);
					merge_stderr = db2.GetField(2);
					use_agent = db2.GetField(3);
					user = db2.GetField(4);
					host = db2.GetField(5);
					wd = db2.GetField(6);
					path = db2.GetField(7);
				}
			}
			
			task.setAttribute("path",path);
			task.removeAttribute("name");
			
			task.setAttribute("parameters-mode",parameters_mode);
			task.setAttribute("output-method",output_method);
			if(merge_stderr!="0")
				task.setAttribute("merge-stderr",merge_stderr);
			if(use_agent!="0")
				task.setAttribute("use-agent",use_agent);
			if(user!="")
				task.setAttribute("user",user);
			if(host!="")
				task.setAttribute("host",host);
			if(wd!="")
				task.setAttribute("wd",wd);
		}
		
		vector<string> xpath_attributes;
		xpath_attributes.push_back("loop");
		xpath_attributes.push_back("condition");
		xpath_attributes.push_back("iteration-condition");
		xpath_attributes.push_back("select");
		for(int i=0;i<xpath_attributes.size();i++)
		{
			string attribute_name = xpath_attributes.at(i);
		
			unique_ptr<DOMXPathResult> xpath_strings(xmldoc->evaluate("//@"+attribute_name,xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
			
			unsigned int xpath_strings_index = 0;
			while(xpath_strings->snapshotItem(xpath_strings_index++))
			{
				DOMNode xpath_string_node = xpath_strings->getNodeValue();
				string xpath_string = xpath_string_node.getNodeValue();
				
				vector<string> function_names;
				function_names.push_back("evqGetOutput");
				function_names.push_back("evqGetInput");
				for(int j=0;j<function_names.size();j++)
				{
					pcrecpp::RE regex(function_names.at(j)+"\\(['\"]([^'\")]+)['\"]");
					pcrecpp::StringPiece str_to_match(xpath_string);
					string match;
					size_t find_start_pos = 0;
					while(regex.FindAndConsume(&str_to_match,&match))
					{
						db2.QueryPrintf("SELECT task_binary FROM t_task WHERE task_name=%s",&match);
						if(db2.FetchRow())
						{
							string task_path = db2.GetField(0);
						
							size_t start_pos = xpath_string.find(match,find_start_pos);
							xpath_string.replace(start_pos,match.length(),task_path);
							
							find_start_pos += start_pos + task_path.length();
						}
					}
				}
				
				xpath_string_node.setTextContent(xpath_string);
			}
		}
		
		string xml2 = xmldoc->Serialize(xmldoc->getDocumentElement());
		db2.QueryPrintf("UPDATE t_workflow SET workflow_xml=%s WHERE workflow_id=%i",&xml2,&id);
	}
	
	db.Query("DROP TABLE IF EXISTS t_task");
	
	for(auto it=evqueue_tables.begin();it!=evqueue_tables.end();++it)
	{
		string version = "v" EVQUEUE_VERSION;
		db.QueryPrintf("ALTER TABLE "+it->first+" COMMENT=%s",&version);
	}
}

void tools_upgrade_v22_v30(void)
{
	Logger::Log(LOG_NOTICE,"Detected v2.2 tables, upgrading scheme to v3.0");
	
	DB db;
	
	// Adding ID as primary key for users
	db.Query("ALTER TABLE t_user DROP PRIMARY KEY");
	db.Query("ALTER TABLE t_user ADD COLUMN `user_id` int(10) unsigned NOT NULL PRIMARY KEY AUTO_INCREMENT FIRST");
	db.Query("ALTER TABLE t_user ADD UNIQUE(user_login)");

	db.Query("ALTER TABLE t_log_api ADD COLUMN user_id INT UNSIGNED NOT NULL AFTER node_name");
	db.Query("ALTER TABLE t_log_api CHANGE COLUMN user_login user_login varchar(32) COLLATE utf8_unicode_ci NOT NULL");
	db.Query("UPDATE t_log_api INNER JOIN t_user ON(t_log_api.user_login=t_user.user_login) SET t_log_api.user_id=t_user.user_id");
	db.Query("ALTER TABLE t_log_api DROP COLUMN user_login");

	db.Query("ALTER TABLE t_user_right DROP INDEX user_login");
	db.Query("ALTER TABLE t_user_right ADD COLUMN user_id INT UNSIGNED NOT NULL FIRST");
	db.Query("UPDATE t_user_right INNER JOIN t_user ON(t_user_right.user_login=t_user.user_login) SET t_user_right.user_id=t_user.user_id");
	db.Query("ALTER TABLE t_user_right DROP COLUMN user_login");
	db.Query("ALTER TABLE t_user_right ADD PRIMARY KEY(user_id, workflow_id)");
	
	// Drop unsed column workflow schedule bound
	db.Query("ALTER TABLE t_workflow DROP COLUMN workflow_bound");
	
	// Update tables version
	for(auto it=evqueue_tables.begin();it!=evqueue_tables.end();++it)
	{
		string version = "v" EVQUEUE_VERSION;
		db.QueryPrintf("ALTER TABLE "+it->first+" COMMENT=%s",&version);
	}
}

void tools_upgrade_v30_v31(void)
{
	Logger::Log(LOG_NOTICE,"Detected v3.0 tables, upgrading scheme to v3.1");
	
	DB db;
	
	// Update tables version
	for(auto it=evqueue_tables.begin();it!=evqueue_tables.end();++it)
	{
		string version = "v" EVQUEUE_VERSION;
		db.QueryPrintf("ALTER TABLE "+it->first+" COMMENT=%s",&version);
	}
}

void tools_upgrade_t_notification()
{
	DB db;
	db.QueryPrintf(
		"SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE table_schema=%s AND table_name='t_notification' AND column_name='notification_subscribe_all'",
		&ConfigurationEvQueue::GetInstance()->Get("mysql.database")
	);
	
	db.FetchRow();
	if(db.GetFieldInt(0)==0)
		db.Query("ALTER TABLE t_notification ADD COLUMN `notification_subscribe_all` int(10) unsigned NOT NULL DEFAULT 0 AFTER notification_name");
}
