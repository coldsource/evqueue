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

#ifndef _CLIENTBASE_H_
#define _CLIENTBASE_H_

#include <string>

#include <DOM/DOMDocument.h>

class XMLResponse;

class ClientBase
{
	bool connected;
	bool authenticated;
	std::string node;
	
	std::string connection_str;
	std::string user;
	std::string password;
	
	enum t_connection_type{tcp,unix};
	t_connection_type connection_type;
	std::string host;
	int port;
	
	int cnx_timeout = 0;
	int snd_timeout = 0;
	int rcv_timeout = 0;

	protected:
		int s = -1;
	
		XMLResponse *response = 0;
	
	public:
		ClientBase(const std::string &connection_str, const std::string &user, const std::string &password);
		virtual ~ClientBase();
		
		const std::string &Connect(void);
		void Disconnect(void);
		
		void Exec(const std::string &cmd);
		DOMDocument *GetResponseDOM();
		XMLResponse *GetResponseHandler() { return response; }
		const std::string &GetNode() { return node; }
		
		void SetTimeouts(int cnx_timeout,int snd_timeout,int rcv_timeout);
		
		static std::string HashPassword(const std::string &password);
	
	protected:
		void connect();
		void send(const std::string &cmd);
		void recv();
		void authenticate();
		void disconnect();
};

#endif
