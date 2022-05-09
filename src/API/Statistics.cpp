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

#include <API/Statistics.h>
#include <API/ActiveConnections.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <Exception/Exception.h>
#include <Queue/QueuePool.h>
#include <User/User.h>
#include <WorkflowInstance/Task.h>
#include <API/QueryHandlers.h>

#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>

Statistics *Statistics::instance = 0;

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("statistics",Statistics::HandleQuery);
	return (APIAutoInit *)0;
});

using namespace std;

Statistics::Statistics(void)
{
	instance = this;
	
	accepted_api_connections = 0;
	accepted_ws_connections = 0;
	api_queries = 0;
	ws_queries = 0;
	ws_events = 0;
	ws_subscriptions = 0;
	api_exceptions = 0;
	workflow_queries = 0;
	workflow_status_queries = 0;
	workflow_cancel_queries = 0;
	statistics_queries = 0;
	workflow_exceptions = 0;
	workflow_instance_launched = 0;
	workflow_instance_executing = 0;
	workflow_instance_errors = 0;
	waiting_threads = 0;
}

unsigned int Statistics::GetAcceptedConnections(void)
{
	unique_lock<mutex> llock(lock);
	return accepted_api_connections + accepted_ws_connections;
}

void Statistics::IncAPIAcceptedConnections(void)
{
	unique_lock<mutex> llock(lock);
	accepted_api_connections++;
}

void Statistics::IncWSAcceptedConnections(void)
{
	unique_lock<mutex> llock(lock);
	accepted_ws_connections++;
}

void Statistics::IncAPIQueries(void)
{
	unique_lock<mutex> llock(lock);
	api_queries++;
}

void Statistics::IncWSQueries(void)
{
	unique_lock<mutex> llock(lock);
	ws_queries++;
}

void Statistics::IncWSEvents(void)
{
	unique_lock<mutex> llock(lock);
	ws_events++;
}

void Statistics::IncWSSubscriptions(void)
{
	unique_lock<mutex> llock(lock);
	ws_subscriptions++;
}

void Statistics::DecWSSubscriptions(int n)
{
	unique_lock<mutex> llock(lock);
	ws_subscriptions-=n;
}

void Statistics::IncAPIExceptions(void)
{
	unique_lock<mutex> llock(lock);
	api_exceptions++;
}

void Statistics::IncWorkflowQueries(void)
{
	unique_lock<mutex> llock(lock);
	workflow_queries++;
}

void Statistics::IncWorkflowStatusQueries(void)
{
	unique_lock<mutex> llock(lock);
	workflow_status_queries++;
}

void Statistics::IncWorkflowCancelQueries(void)
{
	unique_lock<mutex> llock(lock);
	workflow_cancel_queries++;
}

void Statistics::IncStatisticsQueries(void)
{
	unique_lock<mutex> llock(lock);
	statistics_queries++;
}

void Statistics::IncWorkflowExceptions(void)
{
	unique_lock<mutex> llock(lock);
	workflow_exceptions++;
}

void Statistics::IncWorkflowInstanceExecuting(void)
{
	unique_lock<mutex> llock(lock);
	workflow_instance_launched++;
	workflow_instance_executing++;
}

void Statistics::DecWorkflowInstanceExecuting(void)
{
	unique_lock<mutex> llock(lock);
	if(workflow_instance_executing>0)
		workflow_instance_executing--;
}

void Statistics::IncWorkflowInstanceErrors(void)
{
	unique_lock<mutex> llock(lock);
	workflow_instance_errors++;
}

void Statistics::IncWaitingThreads(void)
{
	unique_lock<mutex> llock(lock);
	waiting_threads++;
}

void Statistics::DecWaitingThreads(void)
{
	unique_lock<mutex> llock(lock);
	waiting_threads--;
}

void Statistics::SendGlobalStatistics(QueryResponse *response)
{
	DOMDocument *xmldoc = response->GetDOM();
	
	DOMElement statistics_node = xmldoc->createElement("statistics");
	xmldoc->getDocumentElement().appendChild(statistics_node);
	statistics_node.setAttribute("accepted_api_connections",to_string(accepted_api_connections));
	statistics_node.setAttribute("accepted_ws_connections",to_string(accepted_ws_connections));
	statistics_node.setAttribute("current_api_connections",to_string(ActiveConnections::GetInstance()->GetAPINumber()));
	statistics_node.setAttribute("current_ws_connections",to_string(ActiveConnections::GetInstance()->GetWSNumber()));
	statistics_node.setAttribute("api_queries",to_string(api_queries));
	statistics_node.setAttribute("ws_queries",to_string(ws_queries));
	statistics_node.setAttribute("ws_events",to_string(ws_events));
	statistics_node.setAttribute("ws_subscriptions",to_string(ws_subscriptions));
	statistics_node.setAttribute("api_exceptions",to_string(api_exceptions));
	statistics_node.setAttribute("workflow_queries",to_string(workflow_queries));
	statistics_node.setAttribute("workflow_status_queries",to_string(workflow_status_queries));
	statistics_node.setAttribute("workflow_cancel_queries",to_string(workflow_cancel_queries));
	statistics_node.setAttribute("statistics_queries",to_string(statistics_queries));
	statistics_node.setAttribute("workflow_exceptions",to_string(workflow_exceptions));
	statistics_node.setAttribute("workflow_instance_launched",to_string(workflow_instance_launched));
	statistics_node.setAttribute("workflow_instance_executing",to_string(workflow_instance_executing));
	statistics_node.setAttribute("workflow_instance_errors",to_string(workflow_instance_errors));
	statistics_node.setAttribute("waiting_threads",to_string(waiting_threads));
}

void Statistics::ResetGlobalStatistics()
{
	unique_lock<mutex> llock(lock);
	accepted_api_connections = 0;
	accepted_ws_connections = 0;
	api_queries = 0;
	ws_queries = 0;
	ws_events = 0;
	api_exceptions = 0;
	workflow_queries = 0;
	workflow_status_queries = 0;
	workflow_cancel_queries = 0;
	statistics_queries = 0;
	workflow_exceptions = 0;
	workflow_instance_launched = 0;
	workflow_instance_errors = 0;
}

bool Statistics::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	Statistics *stats = Statistics::GetInstance();
	
	const string action = query->GetRootAttribute("action");
	
	if(action=="query")
	{
		const string type = query->GetRootAttribute("type");
		
		if(type=="global")
		{
			stats->IncStatisticsQueries();
			
			stats->SendGlobalStatistics(response);
			
			return true;
		}
		else if(type=="queue")
		{
			stats->IncStatisticsQueries();
				
			QueuePool *qp = QueuePool::GetInstance();
			qp->SendStatistics(response);
			
			return true;
		}
		else
			throw Exception("Statistics","Unknown statistics type", "UNKNOWN_TYPE");
	}
	else if(action=="reset")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		const string type = query->GetRootAttribute("type");
		
		if(type=="global")
		{
			stats->ResetGlobalStatistics();
			
			return true;
		}
		else
			throw Exception("Statistics","Unknown statistics type for this action","UNKNOWN_TYPE");
	}
	
	return false;
}
