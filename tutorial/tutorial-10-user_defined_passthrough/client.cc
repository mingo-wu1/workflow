/*
  Copyright (c) 2020 Sogou, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

	  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  Author: Xie Han (xiehan@sogou-inc.com;63350856@qq.com)
*/

#include <string.h>
#include <stdio.h>
#include <workflow/Communicator.h>
#include "workflow/Workflow.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/WFFacilities.h"
#include "message.h"

namespace passthrough {
#define HEAD_SIZE 0
#define READ_BUFFER_SIZE 1024
class PassThroughMessage : public CommMessageOut, public CommMessageIn
{
protected:
	virtual int encode(struct iovec vectors[], int max)
	{
		errno = ENOSYS;
		return -1;
	}

	/* You have to implement one of the 'append' functions, and the first one
	 * with arguement 'size_t *size' is recommmended. */

	/* Argument 'size' indicates bytes to append, and returns bytes used. */
	virtual int append(const void *buf, size_t *size)
	{
		return this->append(buf, *size);
	}

	// /* When implementing this one, all bytes are consumed. Cannot support
	//  * streaming protocol. */
	virtual int append(const void *buf, size_t size)
	{
		errno = ENOSYS;
		return -1;
	}

public:
	void set_size_limit(size_t limit) { this->size_limit = limit; }
	size_t get_size_limit() const { return this->size_limit; }

public:
	class Attachment
	{
	public:
		virtual ~Attachment() { }
	};

	void set_attachment(Attachment *att) { this->attachment = att; }
	Attachment *get_attachment() const { return this->attachment; }

protected:
	virtual int feedback(const void *buf, size_t size)
	{
		return this->CommMessageIn::feedback(buf, size);
	}

protected:
	size_t size_limit;

private:
	Attachment *attachment;
};

class TutorialMessage : public PassThroughMessage
{
private:
	virtual int encode(struct iovec vectors[], int max)
	{
		uint32_t n = htonl(this->body_size);
		vectors[0].iov_len = HEAD_SIZE;
		vectors[1].iov_base = this->body;
		vectors[1].iov_len = this->body_size;

		return 2;	/* return the number of vectors used, no more then max. */
	}

	virtual int append(const void *buf, size_t size){
		this->body_size = size;
	    	if (this->body_size > this->size_limit)
	    	{
	    		errno = EMSGSIZE;
	    		return -1;
	    	}
		if(!this->body)
			this->body = (char *)malloc(READ_BUFFER_SIZE);
	    if (!this->body)
	    	return -1;
    
		this->body_received = 0;
    
	    size_t body_left = this->body_size - this->body_received;

		// test print
		// printf("body_size:%d\n", (int)this->body_size);
		// printf("body_received:%d\n", (int)this->body_received);
		// printf("body_left:%d\n", (int)body_left);
		// printf("size:%d\n", (int)size);
		// test

	    if (size > body_left)
	    {
	    	errno = EBADMSG;
	    	return -1;
	    }
    
	    memcpy(this->body, buf, size);
	    if (size < body_left)
	    	return 0;
    
	    return 1;
	}

public:
	int set_message_body(const void *body, size_t size)
	{
		void *p = malloc(size);

		if (!p)
			return -1;

		memcpy(p, body, size);
		free(this->body);
		this->body = (char *)p;
		this->body_size = size;

		this->body_received = size;
		return 0;
	}

	void get_message_body_nocopy(void **body, size_t *size)
	{
		*body = this->body;
		*size = this->body_size;
	}

protected:
	char *body;
	size_t body_received;
	size_t body_size;

public:
	TutorialMessage()
	{
		this->body = NULL;
		this->body_size = 0;
	}

	TutorialMessage(TutorialMessage&& msg):PassThroughMessage(std::move(msg))
	{
		this->body = msg.body;
		this->body_received = msg.body_received;
		this->body_size = msg.body_size;

		msg.body = NULL;
		msg.body_size = 0;
	}

	TutorialMessage& operator = (TutorialMessage&& msg)
	{
		if (&msg != this)
		{
			*(PassThroughMessage *)this = std::move(msg);

			this->body = msg.body;
			this->body_received = msg.body_received;
			this->body_size = msg.body_size;

			msg.body = NULL;
			msg.body_size = 0;
		}
		return *this;
	}

	virtual ~TutorialMessage()
	{
		free(this->body);
	}
};

using TutorialRequest = TutorialMessage;
using TutorialResponse = TutorialMessage;
}

using WFTutorialTask = WFNetworkTask<passthrough::TutorialRequest,
									 passthrough::TutorialResponse>;

using tutorial_callback_t = std::function<void (WFTutorialTask *)>;

class MyFactory : public WFTaskFactory
{
public:
	static WFTutorialTask *create_tutorial_task(const std::string& host,
												unsigned short port,
												int retry_max,
												tutorial_callback_t callback)
	{
		using NTF = WFNetworkTaskFactory<passthrough::TutorialMessage, 
										 passthrough::TutorialMessage>;
		WFTutorialTask *task = NTF::create_client_task(TT_TCP, host, port,
													   retry_max,
													   std::move(callback));
		task->set_keep_alive(30 * 1000);
		
		return task;
	}
};

int main(int argc, char *argv[])
{
	unsigned short port;
	std::string host;

	if (argc != 3)
	{
		fprintf(stderr, "USAGE: %s <host> <port>\n", argv[0]);
		exit(1);
	}

	host = argv[1];
	port = atoi(argv[2]);
	std::function<void (WFTutorialTask *task)> callback =
		[&host, port, &callback](WFTutorialTask *task) {
		int state = task->get_state();
		int error = task->get_error();
		passthrough::TutorialMessage *resp = task->get_resp();
		char buf[1024];
		void *body;
		size_t body_size;

		if (state != WFT_STATE_SUCCESS)
		{
			if (state == WFT_STATE_SYS_ERROR)
				fprintf(stderr, "SYS error: %s\n", strerror(error));
			else if (state == WFT_STATE_DNS_ERROR)
				fprintf(stderr, "DNS error: %s\n", gai_strerror(error));
			else
				fprintf(stderr, "other error.\n");
			return;
		}

		resp->get_message_body_nocopy(&body, &body_size);
		if (body_size != 0)
			printf("Server Response: %.*s\n", (int)body_size, (char *)body);


		printf("Input next request string (Ctrl-D to exit): ");
		*buf = '\0';
		scanf("%1024s", buf);
		body_size = strlen(buf);
		if (body_size > 0)
		{
			WFTutorialTask *next;
			next = MyFactory::create_tutorial_task(host, port, 0, callback);
			next->get_req()->set_message_body(buf, body_size);
			next->get_resp()->set_size_limit(4 * 1024);
			**task << next; /* equal to: series_of(task)->push_back(next) */
		}
		else
			printf("\n");
	};

	/* First request is emtpy. We will ignore the server response. */
	WFFacilities::WaitGroup wait_group(1);
	WFTutorialTask *task = MyFactory::create_tutorial_task(host, port, 0, callback);
	task->get_resp()->set_size_limit(4 * 1024);
	Workflow::start_series_work(task, [&wait_group](const SeriesWork *) {
		wait_group.done();
	});

	wait_group.wait();
	return 0;
}

