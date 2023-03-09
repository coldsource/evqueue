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

#include <Schedule/WorkflowSchedules.h>
#include <Schedule/WorkflowSchedule.h>
#include <DB/DB.h>
#include <Logger/Logger.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <Exception/Exception.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <User/User.h>
#include <API/QueryHandlers.h>


WorkflowSchedules *WorkflowSchedules::instance = 0;

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("workflow_schedules",WorkflowSchedules::HandleQuery);
	return (APIAutoInit *)0;
});

using namespace std;

WorkflowSchedules::WorkflowSchedules():APIObjectList("WorkflowSchedule")
{
	instance = this;
	
	Reload(false);
}

WorkflowSchedules::~WorkflowSchedules()
{
}

void WorkflowSchedules::Reload(bool notify)
{
	Logger::Log(LOG_NOTICE,"Reloading workflow schedules definitions");
	
	unique_lock<mutex> llock(lock);
	
	try
	{
		clear();
		active_schedules.clear();
		
		DB db;
		
		// Build the list of active and inactive schedules
		db.QueryPrintf("SELECT workflow_schedule_id,node_name FROM t_workflow_schedule");
		while(db.FetchRow())
		{
			WorkflowSchedule *workflow_schedule = new WorkflowSchedule(db.GetFieldInt(0));
			
			add(db.GetFieldInt(0),"",workflow_schedule);
			
			// Schedule is actif if set as active AND configured on the current node
			if(workflow_schedule->GetIsActive() && (db.GetField(1)==ConfigurationEvQueue::GetInstance()->Get("cluster.node.name") || db.GetField(1)=="any" || db.GetField(1)=="all"))
				active_schedules.push_back(workflow_schedule);
		}
	}
	catch(Exception &e)
	{
		Logger::Log(LOG_ERR,"[ WorkflowSchedules ] Unexpected exception trying to reload configuration : [ "+e.context+" ] "+e.error);
		Logger::Log(LOG_ERR,"[ WorkflowSchedules ] Configuration reload failed");
		return;
	}
}

const vector<WorkflowSchedule *> &WorkflowSchedules::GetActiveWorkflowSchedules()
{
	return active_schedules;
}

bool WorkflowSchedules::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	WorkflowSchedules *workflow_schedules = WorkflowSchedules::GetInstance();
	
	string action = query->GetRootAttribute("action");
	bool display_parameters = query->GetRootAttributeBool("display_parameters",false);
	
	if(action=="list")
	{
		DB db;
		db.Query("SELECT ws.workflow_schedule_id, ws.node_name, ws.workflow_id, ws.workflow_schedule, ws.workflow_schedule_onfailure, ws.workflow_schedule_user, ws.workflow_schedule_host, ws.workflow_schedule_active, ws.workflow_schedule_comment, w.workflow_name, w.workflow_group FROM t_workflow_schedule ws, t_workflow w WHERE ws.workflow_id=w.workflow_id");
		
		while(db.FetchRow())
		{
			DOMElement node = (DOMElement)response->AppendXML("<workflow_schedule />");
			node.setAttribute("id",db.GetField(0));
			node.setAttribute("node",db.GetField(1));
			node.setAttribute("workflow_id",db.GetField(2));
			node.setAttribute("schedule",db.GetField(3));
			node.setAttribute("onfailure",db.GetField(4));
			if(!db.GetFieldIsNULL(5))
				node.setAttribute("user",db.GetField(5));
			if(!db.GetFieldIsNULL(6))
				node.setAttribute("host",db.GetField(6));
			node.setAttribute("active",db.GetField(7));
			node.setAttribute("comment",db.GetField(8));
			node.setAttribute("workflow_name",db.GetField(9));
			node.setAttribute("workflow_group",db.GetField(10));
			
			if(display_parameters)
			{
				WorkflowSchedule workflow_schedule = WorkflowSchedules::GetInstance()->Get(db.GetFieldInt(0));
				WorkflowParameters *parameters = workflow_schedule.GetParameters();
				
				parameters->SeekStart();
				string name, value;
				while(parameters->Get(name,value))
				{
					DOMElement parameter_node = (DOMElement)response->AppendXML("<parameter />",node);
					parameter_node.setAttribute("name",name);
					parameter_node.setAttribute("value",value);
				}
			}
		}
		
		return true;
	}
	
	return false;
}
