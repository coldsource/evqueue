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

#include <Tag/Tags.h>
#include <Tag/Tag.h>
#include <User/User.h>
#include <Exception/Exception.h>
#include <DB/DB.h>
#include <Logger/Logger.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <Cluster/Cluster.h>
#include <API/QueryHandlers.h>

Tags *Tags::instance = 0;

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("tags", Tags::HandleQuery);
	qh->RegisterReloadHandler("tags", Tags::HandleReload);
	return (APIAutoInit *)new Tags();
});

using namespace std;

Tags::Tags():APIObjectList("Tag")
{
	instance = this;
	
	Reload(false);
}

Tags::~Tags()
{
}

void Tags::Reload(bool notify)
{
	Logger::Log(LOG_NOTICE,"Reloading tags");
	
	unique_lock<mutex> llock(lock);
	
	clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT tag_id,tag_label FROM t_tag");
	
	while(db.FetchRow())
		add(db.GetFieldInt(0),db.GetField(1),new Tag(&db2,db.GetFieldInt(0)));
	
	llock.unlock();
	
	if(notify)
	{
		// Notify cluster
		Cluster::GetInstance()->Notify("<control action='reload' module='tags' notify='no' />\n");
	}
}

bool Tags::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	Tags *tags = Tags::GetInstance();
	
	string action = query->GetRootAttribute("action");
	
	if(action=="list")
	{
		unique_lock<mutex> llock(tags->lock);
		
		for(auto it = tags->objects_name.begin(); it!=tags->objects_name.end(); it++)
		{
			Tag it_tag = *it->second;
			DOMElement node = (DOMElement)response->AppendXML("<tag />");
			node.setAttribute("id",to_string(it_tag.GetID()));
			node.setAttribute("label",it_tag.GetLabel());
		}
		
		return true;
	}
	
	return false;
}

void Tags::HandleReload(bool notify)
{
	Tags *tags = Tags::GetInstance();
	tags->Reload(notify);
}
