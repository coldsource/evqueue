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

#include <Workflow.h>
#include <DB.h>
#include <Exception.h>
#include <XMLUtils.h>
#include <global.h>
#include <base64.h>

#include <string.h>

using namespace std;

extern string workflow_xsd_str;

Workflow::Workflow()
{
	workflow_id = -1;
}

Workflow::Workflow(DB *db,const char *workflow_name)
{
	db->QueryPrintf("SELECT workflow_id,workflow_name,workflow_xml FROM t_workflow WHERE workflow_name=%s",workflow_name);
	
	if(!db->FetchRow())
		throw Exception("Workflow","Unknown Workflow");
	
	workflow_id = db->GetFieldInt(0);
	
	this->workflow_name = db->GetField(1);
	workflow_xml = db->GetField(2);
	
	db->QueryPrintf("SELECT notification_id FROM t_workflow_notification WHERE workflow_id=%i",&workflow_id);
	while(db->FetchRow())
		notifications.push_back(db->GetFieldInt(0));
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

void Workflow::Create(const string &name, const string &base64, const string &group, const string &comment)
{
	string xml = create_edit_check(name,base64,group,comment);
	
	DB db;
	db.QueryPrintf("INSERT INTO t_workflow(workflow_name,workflow_xml,workflow_group,workflow_comment) VALUES(%s,%s,%s,%s)",
		name.c_str(),
		xml.c_str(),
		group.c_str(),
		comment.c_str()
	);
}

void Workflow::Edit(const string &name, const string &base64, const string &group, const string &comment)
{
	string xml = create_edit_check(name,base64,group,comment);
	
	DB db;
	db.QueryPrintf("UPDATE t_workflow SET workflow_name=%s,workflow_xml=%s,workflow_group=%s,workflow_comment=%s WHERE workflow_name=%s",
		name.c_str(),
		xml.c_str(),
		group.c_str(),
		comment.c_str(),
		name.c_str()
	);
	
	if(db.AffectedRows()==0)
		throw Exception("Workflow","Workflow not found");
}

void Workflow::Delete(const string &name)
{
	DB db;
	db.QueryPrintf("DELETE FROM t_workflow WHERE workflow_name=%s",name.c_str());
	
	if(db.AffectedRows()==0)
		throw Exception("Workflow","Workflow not found");
}

string Workflow::create_edit_check(const string &name, const string &base64, const string &group, const string &comment)
{
	if(!CheckWorkflowName(name))
		throw Exception("Workflow","Invalid workflow name");
	
	string workflow_xml;
	if(!base64_decode_string(workflow_xml,base64))
		throw Exception("Workflow","Invalid base64 sequence");
	
	XMLUtils::ValidateXML(workflow_xml,workflow_xsd_str);
	
	return workflow_xml;
}