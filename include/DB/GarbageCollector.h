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

#ifndef _GARBAGECOLLECTOR_H_
#define _GARBAGECOLLECTOR_H_

#include <Thread/WaiterThread.h>

#include <thread>
#include <mutex>
#include <condition_variable>

class GarbageCollector: public WaiterThread
{
	private:
		bool enable;
		int delay;
		int interval;
		int limit;
		int workflowinstance_retention;
		int logs_retention;
		int logsapi_retention;
		int logsnotifications_retention;
		int uniqueaction_retention;
		int elogs_retention;
		std::string dbname;
		
		std::thread gc_thread_handle;
		
	public:
		GarbageCollector();
		~GarbageCollector();
		
		void WaitForShutdown(void);
	
	private:
		static void *gc_thread(GarbageCollector *gc);
		
		int purge(time_t now);
};

#endif
