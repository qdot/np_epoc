/*
 * Implementation file for Emotiv EPOC Max/Pd External
 *
 * Copyright (c) 2010 Kyle Machulis/Nonpolynomial Labs <kyle@nonpolynomial.com>
 *
 * More info on Nonpolynomial Labs @ http://www.nonpolynomial.com/externals
 *
 * Project @ http://www.github.com/qdot/np_epoc
 *
 * Example code from flext tutorials. http://www.parasitaere-kapazitaeten.net/ext/flext/
 */


#ifndef FLEXT_THREADS
#define FLEXT_THREADS
#endif

// include flext header
#include <flext.h>

//god damnit, cycling '74.
#ifdef PI
#undef PI
#endif

// check for appropriate flext version
#if !defined(FLEXT_VERSION) || (FLEXT_VERSION < 400)
#error You need at least flext version 0.4.0
#endif


#include <iostream>
#include <string>
#include "libepoc.h"

class np_epoc:
	// inherit from basic flext class
	public flext_base
{
	// obligatory flext header (class name,base class name)
	FLEXT_HEADER(np_epoc,flext_base)
	// Same as boost ScopedMutex, just using flext's mutex class.
	class ScopedMutex
	{
		ScopedMutex() {}

	public:
		ScopedMutex(ThrMutex& tm)
		{
			m = &tm;
			m->Lock();
		}

		~ScopedMutex()
		{
			m->Unlock();
		}
	private:
		ThrMutex* m;
	};

public:
	// constructor
	np_epoc() :
		m_runThread(false),
		m_sleepTime(.01),
		m_vid(0),
		m_pid(0),
		m_key(0)
	{

		// external setup
		AddInAnything("Command Input");
		AddOutBang("Bangs on successful connection/command");
		AddOutInt("Gyro X");
		AddOutInt("Gyro Y");
		AddOutInt("Raw Wave (14 outputs: F3, FC6, P7, T8, F7, F8, T7, P8, AF4, F4, AF3, O2, O1, FC5)");

		//FLEXT_ADDBANG(0, epoc_output);
		FLEXT_ADDMETHOD_(0, "start", epoc_start);
		FLEXT_ADDMETHOD_(0, "stop", epoc_stop);
		FLEXT_ADDMETHOD_(0, "count", epoc_count);
		FLEXT_ADDMETHOD_(0, "headset", epoc_anything);
		//FLEXT_ADDMETHOD_(0, "close", epoc_close);

		post("Neurosky Epoc External v0.1.5");
		post("RESEARCH HEADSET ONLY w/ 0x1234 DONGLE ONLY");
		post("by Nonpolynomial Labs (http://www.nonpolynomial.com)");
		post("Updates at http://www.github.com/qdot/np_epoc");
		post("Compiled on " __DATE__ " " __TIME__);

		epoc_init(RESEARCH_HEADSET);
		m_epoc = epoc_create();
	}

	virtual void Exit()
	{
		if(m_runThread)
			epoc_stop();
		flext_base::Exit();
	}

	virtual ~np_epoc()
	{
	}

protected:

	volatile bool m_hasUpdated;
	volatile bool m_runThread;
	volatile bool m_outputRaw;
	volatile float m_sleepTime;
	ThrMutex m_epocMutex;
	ThrMutex m_threadMutex;
	epoc_device* m_epoc;
	int m_vid;
	int m_pid;
	int m_key;

	void epoc_anything(const t_symbol *msg,int argc,t_atom *argv)
	{
		if(!strcmp(msg->s_name, "headset"))
		{
			if(argc != 3)
			{				
				post("np_epoc - headset message takes 3 arguments: VID, PID, Key Index");
				return;
			}
			m_key = GetInt(argv[2]);
			if(m_key > 2 || m_key < 0)
			{
				post("np_epoc - Key must be 0 (consumer), 1 (research), 2 (special)");
				return;
			}			
			m_vid = GetInt(argv[0]);
			m_pid = GetInt(argv[1]);
			epoc_init((headset_type)m_key);
			post("np_epoc - Headset info - VID: 0x%.04x PID: 0x%.04x Key %d", m_vid, m_pid, m_key);
			return;
		}
		post("np_epoc - not a valid np_epoc message: %s", msg->s_name);
	}

	void epoc_count()
	{
		if(m_vid == 0 || m_pid == 0)
		{
			post("np_epoc - Must use headset command to set VID/PID first");
			return;
		}
		post("np_epoc - Devices connected: %d", epoc_get_count(m_epoc, m_vid, m_pid));
	}
	
	void epoc_stop()
	{
		m_runThread = false;
		StopThreads();
		ToOutBang(0);
	}

	void epoc_start()
	{
		if(!m_threadMutex.TryLock())
		{
			post("np_epoc - Thread already running");
			return;
		}
		m_threadMutex.Unlock();
		ScopedMutex m(m_threadMutex);
		post("np_epoc - Entering epoc thread");
		if(m_vid == 0 || m_pid == 0)
		{
			post("np_epoc - Must use headset command to set VID/PID first");
			return;
		}
		if(epoc_open(m_epoc, m_vid, m_pid, 0) != 0)
		{
			post("np_epoc - Cannot open 0x%.04x 0x%.04x, exiting", m_vid, m_pid);
			return;
		}
		post("np_epoc - Opened Device");
		ToOutBang(0);
		m_runThread = true;
		unsigned char data[32];
		struct epoc_frame frame;
		t_atom raw_list[14];

		while(!ShouldExit() && m_runThread)
		{
			if(epoc_read_data(m_epoc, data) > 0)
			{
				epoc_get_next_frame(&frame, data);
				SetInt(raw_list[0], frame.F3);
				SetInt(raw_list[1], frame.FC6);
				SetInt(raw_list[2], frame.P7);
				SetInt(raw_list[3], frame.T8);
				SetInt(raw_list[4], frame.F7);
				SetInt(raw_list[5], frame.F8);
				SetInt(raw_list[6], frame.T7);
				SetInt(raw_list[7], frame.P8);
				SetInt(raw_list[8], frame.AF4);
				SetInt(raw_list[9], frame.F4);
				SetInt(raw_list[10], frame.AF3);
				SetInt(raw_list[11], frame.O2);
				SetInt(raw_list[12], frame.O1);
				SetInt(raw_list[13], frame.FC5);												
				Lock();
				ToOutInt(1, frame.gyroX);
				ToOutInt(2, frame.gyroY);
				ToOutList(3, 14, raw_list);
				Unlock();
				//epoc_output();
			}
			Sleep(m_sleepTime);
		}
		epoc_close(m_epoc);
		post("np_epoc - Exiting epoc thread");
	}

private:
	FLEXT_CALLBACK_A(epoc_anything)
	FLEXT_THREAD(epoc_start)
	FLEXT_CALLBACK(epoc_stop)
	FLEXT_CALLBACK(epoc_count)
};

FLEXT_NEW("np_epoc", np_epoc)
