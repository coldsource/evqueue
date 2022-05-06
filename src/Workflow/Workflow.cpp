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

#include <Workflow/Workflow.h>
#include <Workflow/Workflows.h>
#include <Workflow/WorkflowParameters.h>
#include <Notification/Notifications.h>
#include <DB/DB.h>
#include <Exception/Exception.h>
#include <XML/XMLUtils.h>
#include <Logger/Logger.h>
#include <Logger/LoggerAPI.h>
#include <Crypto/Sha1String.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <Schedule/WorkflowScheduler.h>
#include <User/Users.h>
#include <Git/Git.h>
#include <User/User.h>
#include <XML/XMLFormatter.h>
#include <global.h>
#include <Crypto/base64.h>
#include <DOM/DOMDocument.h>
#include <WS/Events.h>
#include <API/QueryHandlers.h>

#include <string.h>

#include <memory>

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("workflow",Workflow::HandleQuery);
	Events::GetInstance()->RegisterEvents({"WORKFLOW_CREATED","WORKFLOW_MODIFIED","WORKFLOW_REMOVED","WORKFLOW_SUBSCRIBED","WORKFLOW_UNSUBSCRIBED"});
	return (APIAutoInit *)0;
});

using namespace std;

extern string workflow_xsd_str;

Workflow::Workflow()
{
	workflow_id = -1;
}

Workflow::Workflow(DB *db,const string &workflow_name)
{
	db->QueryPrintf("SELECT workflow_id,workflow_name,workflow_xml, workflow_group, workflow_comment, workflow_lastcommit FROM t_workflow WHERE workflow_name=%s",&workflow_name);
	
	if(!db->FetchRow())
		throw Exception("Workflow","Unknown Workflow");
	
	workflow_id = db->GetFieldInt(0);
	
	this->workflow_name = db->GetField(1);
	workflow_xml = db->GetField(2);
	group = db->GetField(3);
	comment = db->GetField(4);
	lastcommit = db->GetField(5);
	
	db->QueryPrintf("SELECT notification_id FROM t_workflow_notification WHERE workflow_id=%i",&workflow_id);
	while(db->FetchRow())
		notifications.push_back(db->GetFieldInt(0));
}

void Workflow::CheckInputParameters(WorkflowParameters *parameters)
{
	// Load workflow XML
	unique_ptr<DOMDocument> xmldoc(DOMDocument::Parse(workflow_xml));
	
	string parameter_name;
	string parameter_value;
	int passed_parameters = 0;
	
	parameters->SeekStart();
	while(parameters->Get(parameter_name,parameter_value))
	{
		unique_ptr<DOMXPathResult> res(xmldoc->evaluate("parameters/parameter[@name = '"+parameter_name+"']",xmldoc->getDocumentElement(),DOMXPathResult::FIRST_RESULT_TYPE));
		if(!res->isNode())
			throw Exception("Workflow","Unknown parameter : "+parameter_name,"INVALID_WORKFLOW_PARAMETERS");
		
		passed_parameters++;
	}
	
	unique_ptr<DOMXPathResult> res(xmldoc->evaluate("count(parameters/parameter)",xmldoc->getDocumentElement(),DOMXPathResult::FIRST_RESULT_TYPE));
	int workflow_template_parameters = res->getIntegerValue();
	
	if(workflow_template_parameters!=passed_parameters)
	{
		char e[256];
		sprintf(e, "Invalid number of parameters. Workflow expects %d, but %d are given.",workflow_template_parameters,passed_parameters);
		throw Exception("Workflow",e,"INVALID_WORKFLOW_PARAMETERS");
	}
}

bool Workflow::GetIsModified()
{
#ifndef USELIBGIT2
	return false;
#else
	// We are not bound to git, so we cannot compute this
	if(lastcommit=="")
		return false;
	
	// Check SHA1 between repo and current instance
	Git *git = Git::GetInstance();
	
	string repo_hash;
	try {
		repo_hash = git->GetWorkflowHash(lastcommit,workflow_name);
	} catch(Exception &e) {
		return true; // Errors can happen on repo change, ignore them
	}
	
	// Ensure uniform format
	XMLFormatter formatter(SaveToXML());
	formatter.Format(false);
	
	string db_hash = Sha1String(formatter.GetOutput()).GetBinary();
	
	return (repo_hash!=db_hash);
#endif
}

void Workflow::SetLastCommit(const std::string &commit_id)
{
	DB db;
	
	db.QueryPrintf("UPDATE t_workflow SET workflow_lastcommit=%s WHERE workflow_id=%i",commit_id.length()?&commit_id:0,&workflow_id);
}

string Workflow::SaveToXML()
{
	DOMDocument xmldoc;
	
	// Generate workflow XML
	DOMElement node = (DOMElement)XMLUtils::AppendXML(&xmldoc, xmldoc, "<workflow />");
	XMLUtils::AppendXML(&xmldoc, node, workflow_xml);
	node.setAttribute("group",group);
	node.setAttribute("comment",comment);
	
	return xmldoc.Serialize(xmldoc.getDocumentElement());
}

void Workflow::LoadFromXML(string name, DOMDocument *xmldoc, string repo_lastcommit)
{
	DOMElement root_node = xmldoc->getDocumentElement();
		
	string group = XMLUtils::GetAttribute(root_node, "group", true);
	string comment = XMLUtils::GetAttribute(root_node, "comment", true);
	
	string workflow_xml = xmldoc->Serialize(root_node.getFirstChild());
	
	string content;
	base64_encode_string(workflow_xml, content);
	
	if(Workflows::GetInstance()->Exists(name))
	{
		// Update
		Logger::Log(LOG_INFO,string("Updating workflow "+name+" from git").c_str());
		
		Workflow workflow = Workflows::GetInstance()->Get(name);
		
		Workflow::Edit(workflow.GetID(), name, content, group, comment);
		workflow.SetLastCommit(repo_lastcommit);
		
		Workflows::GetInstance()->Reload();
	}
	else
	{
		// Create
		Logger::Log(LOG_INFO,string("Crerating workflow "+name+" from git").c_str());
		
		unsigned int id = Workflow::Create(name, content, group, comment, repo_lastcommit);
		
		Workflows::GetInstance()->Reload();
	}
}

bool Workflow::CheckWorkflowName(const string &workflow_name)
{
	int i,len;
	
	len = workflow_name.length();
	if(len==0 || len>WORKFLOW_NAME_MAX_LEN)
		return false;
	
	for(i=0;i<len;i++)
		if(!isalnum(workflow_name[i]) && workflow_name[i]!='_' && workflow_name[i]!='-')
			return false;
	
	return true;
}

void Workflow::Get(unsigned int id, QueryResponse *response)
{
	Workflow workflow = Workflows::GetInstance()->Get(id);
	get(workflow,response);
}

void Workflow::get(const Workflow &workflow, QueryResponse *response)
{
	DOMElement node = (DOMElement)response->AppendXML("<workflow />");
	response->AppendXML(workflow.GetXML(), node);
	node.setAttribute("name",workflow.GetName());
	node.setAttribute("group",workflow.GetGroup());
	node.setAttribute("comment",workflow.GetComment());
	node.setAttribute("id",to_string(workflow.GetID()));
}

unsigned int Workflow::Create(const string &name, const string &base64, const string &group, const string &comment, const string &lastcommit)
{
	string xml = create_edit_check(name,base64,group,comment);
	
	DB db;
	db.QueryPrintf("SELECT COUNT(*) FROM t_workflow WHERE workflow_name=%s",&name);
	db.FetchRow();
	if(db.GetFieldInt(0)!=0)
		throw Exception("Workflow","Workflow name already exists","WORKFLOW_ALREADY_EXISTS"); 
	
	db.QueryPrintf("INSERT INTO t_workflow(workflow_name,workflow_xml,workflow_group,workflow_comment,workflow_lastcommit) VALUES(%s,%s,%s,%s,%s)",
		&name,
		&xml,
		&group,
		&comment,
		lastcommit.length()?&lastcommit:0
	);
	
	unsigned int workflow_id = db.InsertID();
	
	// Automatically subscribe to notifications
	DB db2(&db);
	db.Query("SELECT notification_id FROM t_notification WHERE notification_subscribe_all=1");
	while(db.FetchRow())
	{
		unsigned int notification_id = db.GetFieldInt(0);
		db2.QueryPrintf("INSERT INTO t_workflow_notification(workflow_id,notification_id) VALUES(%i,%i)",&workflow_id,&notification_id);
	}
	
	Workflows::GetInstance()->Reload();
	
	return workflow_id;
}

void Workflow::Edit(unsigned int id,const string &name, const string &base64, const string &group, const string &comment)
{
	string xml = create_edit_check(name,base64,group,comment);
	
	if(!Workflows::GetInstance()->Exists(id))
		throw Exception("Workflow","Workflow not found","UNKNOWN_WORKFLOW");
	
	DB db;
	db.QueryPrintf("UPDATE t_workflow SET workflow_name=%s,workflow_xml=%s,workflow_group=%s,workflow_comment=%s WHERE workflow_id=%i",
		&name,
		&xml,
		&group,
		&comment,
		&id
	);
	
	Workflows::GetInstance()->Reload();
}

void Workflow::Delete(unsigned int id)
{
	DB db;
	
	bool schedule_deleted = false, rights_deleted = false;
	
	db.StartTransaction();
	
	db.QueryPrintf("DELETE FROM t_workflow WHERE workflow_id=%i",&id);
	
	if(db.AffectedRows()==0)
		throw Exception("Workflow","Workflow not found","UNKNOWN_WORKFLOW");
	
	// Clean notifications
	db.QueryPrintf("DELETE FROM t_workflow_notification WHERE workflow_id=%i",&id);
	
	// Clean schedules
	db.QueryPrintf("DELETE FROM t_workflow_schedule WHERE workflow_id=%i",&id);
	if(db.AffectedRows()>0)
		schedule_deleted = true;
	
	// Delete user rights associated
	db.QueryPrintf("DELETE FROM t_user_right WHERE workflow_id=%i",&id);
	if(db.AffectedRows()>0)
		rights_deleted = true;
	
	db.CommitTransaction();
	
	Workflows::GetInstance()->Reload();
	
	if(schedule_deleted)
		WorkflowScheduler::GetInstance()->Reload();
	
	if(rights_deleted)
		Users::GetInstance()->Reload();
}

void Workflow::ListNotifications(unsigned int id, QueryResponse *response)
{
	Workflow workflow = Workflows::GetInstance()->Get(id);
	
	for(int i=0;i<workflow.notifications.size();i++)
	{
		DOMElement node = (DOMElement)response->AppendXML("<notification />");
		node.setAttribute("id",to_string(workflow.notifications.at(i)));
	}
}

void Workflow::SubscribeNotification(unsigned int id, unsigned int notification_id)
{
	if(!Notifications::GetInstance()->Exists(notification_id))
		throw Exception("Workflow","Notification ID not found","UNKNOWN_NOTIFICATION");
	
	DB db;
	db.QueryPrintf("INSERT INTO t_workflow_notification(workflow_id,notification_id) VALUES(%i,%i)",&id,&notification_id);
	
	Workflows::GetInstance()->Reload();
}

void Workflow::UnsubscribeNotification(unsigned int id, unsigned int notification_id)
{
	DB db;
	db.QueryPrintf("DELETE FROM t_workflow_notification WHERE workflow_id=%i AND notification_id=%i",&id,&notification_id);
	
	if(db.AffectedRows()==0)
		throw Exception("Workflow","Workflow was not subscribed to this notification","UNKNOWN_NOTIFICATION");
	
	Workflows::GetInstance()->Reload();
}

void Workflow::ClearNotifications(unsigned int id)
{
	DB db;
	db.QueryPrintf("DELETE FROM t_workflow_notification WHERE workflow_id=%i",&id);
	
	Workflows::GetInstance()->Reload();
}

string Workflow::create_edit_check(const string &name, const string &base64, const string &group, const string &comment)
{
	if(name=="")
		throw Exception("Workflow","Workflow name cannot be empty","INVALID_PARAMETER");
	
	if(!CheckWorkflowName(name))
		throw Exception("Workflow","Invalid workflow name","INVALID_PARAMETER");

	string workflow_xml;
	if(!base64_decode_string(workflow_xml,base64))
		throw Exception("Workflow","Invalid base64 sequence","INVALID_PARAMETER");
	
	// This function has human readable errors, do these checks first
	ValidateXML(workflow_xml);
	
	// XLM sanity check against XSD
	XMLUtils::ValidateXML(workflow_xml,workflow_xsd_str);
	
	return workflow_xml;
}

unsigned int Workflow::get_id_from_query(XMLQuery *query)
{
	unsigned int id = query->GetRootAttributeInt("id", 0);
	if(id!=0)
		return id;
	
	string name = query->GetRootAttribute("name","");
	if(name=="")
		throw Exception("Workflow","Missing 'id' or 'name' attribute","MISSING_PARAMETER");
	
	return Workflows::GetInstance()->Get(name).GetID();
}

bool Workflow::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	const string action = query->GetRootAttribute("action");
	
	if(action=="get")
	{
		unsigned int id = get_id_from_query(query);
		
		if(!user.HasAccessToWorkflow(id, "read"))
			User::InsufficientRights();
		
		Get(id,response);
		
		return true;
	}
	else if(action=="create" || action=="edit")
	{
		string name = query->GetRootAttribute("name");
		string content = query->GetRootAttribute("content");
		string group = query->GetRootAttribute("group","");
		string comment = query->GetRootAttribute("comment","");
		
		if(action=="create")
		{
			if(!user.IsAdmin())
				User::InsufficientRights();
			
			unsigned int id = Create(name, content, group, comment);
			
			LoggerAPI::LogAction(user,id,"Workflow",query->GetQueryGroup(),action);
			
			response->GetDOM()->getDocumentElement().setAttribute("workflow-id",to_string(id));
			
			Events::GetInstance()->Create("WORKFLOW_CREATED", id);
			Events::GetInstance()->Create("WORKFLOW_SUBSCRIBED", id); // Workflow might have been subscribed to auto notifications
		}
		else
		{
			unsigned int id = get_id_from_query(query);
			
			if(!user.HasAccessToWorkflow(id, "edit"))
				User::InsufficientRights();
			
			Edit(id, name, content, group, comment);
			
			LoggerAPI::LogAction(user,id,"Workflow",query->GetQueryGroup(),action);
			
			Events::GetInstance()->Create("WORKFLOW_MODIFIED", id);
		}
		
		return true;
	}
	else if(action=="delete")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		unsigned int id = get_id_from_query(query);
		
		Delete(id);
		
		LoggerAPI::LogAction(user,id,"Workflow",query->GetQueryGroup(),action);
		
		Events::GetInstance()->Create("WORKFLOW_REMOVED", id);
		
		return true;
	}
	else if(action=="subscribe_notification" || action=="unsubscribe_notification" || action=="clear_notifications" || action=="list_notifications")
	{
		unsigned int id = get_id_from_query(query);
		
		if(action=="clear_notifications")
		{
			if(!user.HasAccessToWorkflow(id, "edit"))
				User::InsufficientRights();
			
			ClearNotifications(id);
			
			LoggerAPI::LogAction(user,id,"Workflow",query->GetQueryGroup(),action);
			
			Events::GetInstance()->Create("WORKFLOW_UNSUBSCRIBED", id);
			
			return true;
		}
		else if(action=="list_notifications")
		{
			if(!user.HasAccessToWorkflow(id, "read"))
				User::InsufficientRights();
			
			ListNotifications(id, response);
			
			return true;
		}
		else if(action=="subscribe_notification" || action=="unsubscribe_notification")
		{
			if(!user.HasAccessToWorkflow(id, "edit"))
				User::InsufficientRights();
			
			unsigned int notification_id = query->GetRootAttributeInt("notification_id");
						
			if(action=="subscribe_notification")
			{
				SubscribeNotification(id,notification_id);
				
				Events::GetInstance()->Create("WORKFLOW_SUBSCRIBED", id);
			}
			else
			{
				UnsubscribeNotification(id,notification_id);
				
				Events::GetInstance()->Create("WORKFLOW_UNSUBSCRIBED", id);
			}
			
			LoggerAPI::LogAction(user,id,"Workflow",query->GetQueryGroup(),action);
			
			return true;
		}
	}
	
	return false;
}

void Workflow::ValidateXML(const string &xml_str)
{
	unique_ptr<DOMDocument> xmldoc(DOMDocument::Parse(xml_str));

	// Check tasks types
	// Ensure that they have either a 'name' or a 'path' attribute
	// Ensure script is not empty
	unique_ptr<DOMXPathResult> tasks(xmldoc->evaluate("//task",xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
	int tasks_index = 0;
	DOMElement task;
	while(tasks->snapshotItem(tasks_index++))
	{
		task = (DOMElement)tasks->getNodeValue();
		string type = "BINARY";
		if(task.hasAttribute("type"))
			type = task.getAttribute("type");
		
		if(type!="BINARY" && type!="SCRIPT")
			throw Exception("Workflow", "Invalid type attribute value '"+type+"'. Muse be 'BINARY' or 'SCRIPT'.");
		
		if(type=="BINARY" && (!task.hasAttribute("path") || task.getAttribute("path")==""))
			throw Exception("Workflow", "Binary task must have a (non empty) 'path' attribute");
		else if(type=="SCRIPT")
		{
			if(!task.hasAttribute("name") || task.getAttribute("name")=="")
				throw Exception("Workflow", "Script task must have a (non empty) 'name' attribute");
			
			unique_ptr<DOMXPathResult> scripts(xmldoc->evaluate("./script",task,DOMXPathResult::FIRST_RESULT_TYPE));
			if(!scripts->isNode())
				throw Exception("Workflow", "Script tasks must have a script node");
			
			DOMElement script = (DOMElement)scripts->getNodeValue();
			
			unique_ptr<DOMXPathResult> values(xmldoc->evaluate("./value",script,DOMXPathResult::FIRST_RESULT_TYPE));
			
			if(script.getTextContent()=="" && !values->isNode())
				throw Exception("Workflow", "Script cannot be empty");
		}
	}
	
	// Check jobs
	// Ensure no jobs are empty
	unique_ptr<DOMXPathResult> jobs(xmldoc->evaluate("//job",xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
	int jobs_index = 0;
	DOMElement job;
	while(jobs->snapshotItem(jobs_index++))
	{
		job = (DOMElement)jobs->getNodeValue();
		
		unique_ptr<DOMXPathResult> ntasks(xmldoc->evaluate("count(tasks/task)",job,DOMXPathResult::FIRST_RESULT_TYPE));
		if(ntasks->getIntegerValue()==0)
			throw Exception("Workflow","A job cannot be empty");
	}
}
