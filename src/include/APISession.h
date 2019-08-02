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

#ifndef _APISESSION_H_
#define _APISESSION_H_

#include <User.h>
#include <AuthHandler.h>
#include <QueryResponse.h>

#include <libwebsockets.h>

#include <string>

class SocketQuerySAX2Handler;

class APISession
{
	public:
		enum en_status
		{
			INITIALIZED,
			WAITING_CHALLENGE_RESPONSE,
			AUTHENTICATED,
			READY,
			QUERY_RECEIVED,
		};
	
	private:
		en_status status;
		std::string context;
		
		std::string remote_host;
		int remote_port;
		
		AuthHandler ah;
		User user;
		
		bool is_xpath_response;
		QueryResponse response;
		QueryResponse xpath_response;
		
		int s;
		struct lws *wsi;
		
		void init(const std::string &context);
	
	public:
		APISession(const std::string &context, int s);
		APISession(const std::string &context, struct lws *wsi);
		
		const User &GetUser() { return user; }
		en_status GetStatus() { return status; }
		
		void SendChallenge();
		void ChallengeReceived(SocketQuerySAX2Handler *saxh);
		
		void SendGreeting();
		
		bool QueryReceived(SocketQuerySAX2Handler *saxh);
		void SendResponse();
		void Query(const std::string &xml);
};

#endif
