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

#include <IO/Filesystem.h>
#include <Exception/Exception.h>
#include <API/QueryResponse.h>
#include <API/XMLQuery.h>
#include <User/User.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <API/QueryHandlers.h>

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("filesystem", Filesystem::HandleQuery);
	return (APIAutoInit *)0;
});

using namespace std;

void Filesystem::List(const string &path,QueryResponse *response)
{
	string full_path;
	if(path[0]=='/')
		full_path = path;
	else
		full_path = ConfigurationEvQueue::GetInstance()->Get("processmanager.tasks.directory")+"/"+path;
		
	DIR *dh = opendir(full_path.c_str());
	if(!dh)
		throw Exception("Filesystem","Unable to open directory "+full_path);
	
	struct dirent *result;
	
	while(true)
	{
		errno = 0;
		result = readdir(dh);
		if(result==0 && errno!=0)
		{
			closedir(dh);
			throw Exception("Filesystem","Unable to read directory "+full_path);
		}
		
		if(result==0)
			break;
		
		if(result->d_name[0]=='.')
			continue; // Skip hidden files
		
		DOMElement node = (DOMElement)response->AppendXML("<entry />");
		node.setAttribute("name",result->d_name);
		if(result->d_type==DT_DIR)
			node.setAttribute("type","directory");
		else if(result->d_type==DT_REG || result->d_type==DT_LNK)
			node.setAttribute("type","file");
		else
			node.setAttribute("type","unknown");
	}
	
	closedir(dh);
}
		
bool Filesystem::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	const string action = query->GetRootAttribute("action");
	
	if(action=="list")
	{
		string path = query->GetRootAttribute("path");
		
		List(path,response);
		
		return true;
	}
	
	return false;
}
