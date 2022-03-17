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

#include <Logger/Logger.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <DB/DB.h>
#include <Exception/Exception.h>
#include <WS/Events.h>

#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

Logger *Logger::instance = 0;

using namespace std;

Logger::Logger():
	node_name(ConfigurationEvQueue::GetInstance()->Get("cluster.node.name"))
{
	Configuration *config = ConfigurationEvQueue::GetInstance();
	
	log_syslog = ConfigurationEvQueue::GetInstance()->GetBool("logger.syslog.enable");
	syslog_filter = parse_log_level(ConfigurationEvQueue::GetInstance()->Get("logger.syslog.filter"));
	
	log_db = ConfigurationEvQueue::GetInstance()->GetBool("logger.db.enable");
	db_filter = parse_log_level(ConfigurationEvQueue::GetInstance()->Get("logger.db.filter"));
	
	instance = this;
}

void Logger::Log(int level,const char *msg,...)
{
	char buf[1024];
	va_list ap;
	int n;
	
	if((!instance->log_syslog || level>instance->syslog_filter) && (!instance->log_db || level>instance->db_filter))
		return;
	
	va_start(ap, msg);
	n = vsnprintf(buf,1024,msg,ap);
	va_end(ap);
	
	if(n<0)
		return;
	
	Log(level,string(buf));
}

void Logger::Log(int level,const string &msg)
{
	if(instance->log_syslog && level<=instance->syslog_filter)
		syslog(LOG_NOTICE,"%s",msg.c_str());
	
	if(instance->log_db && level<=instance->db_filter)
	{
		try
		{
			DB db;
			db.QueryPrintfC("INSERT INTO t_log(node_name,log_level,log_message,log_timestamp) VALUES(%s,%i,%s,NOW())",instance->node_name.c_str(),&level,msg.c_str());
			
			if(Events::GetInstance())
				Events::GetInstance()->Create(Events::en_types::LOG_ENGINE);
		}
		catch(Exception &e) { } // Logger should never send exceptions on database error to prevent exception storm
	}
}

int Logger::parse_log_level(const string &log_level)
{
	if(log_level=="LOG_EMERG")
		return LOG_EMERG;
	else if(log_level=="LOG_ALERT")
		return LOG_ALERT;
	else if(log_level=="LOG_CRIT")
		return LOG_CRIT;
	else if(log_level=="LOG_ERR")
		return LOG_ERR;
	else if(log_level=="LOG_WARNING")
		return LOG_WARNING;
	else if(log_level=="LOG_NOTICE")
		return LOG_NOTICE;
	else if(log_level=="LOG_INFO")
		return LOG_INFO;
	else if(log_level=="LOG_DEBUG")
		return LOG_DEBUG;
	
	throw Exception("Logger","Unknown filter level");
}
